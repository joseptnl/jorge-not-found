#include "HM25.h"
#include <iostream>
#include <ctime>

// Constants for MSVAULT contract
const uint64_t MSVAULT_CONTRACT_INDEX = 11;
const uint64_t MSVAULT_HOLDING_FEE = 1000; // Assuming a fee value
const uint64_t MSVAULT_MAX_OWNERS = 2;

HM25::HM25() 
    : nextProjectId(0), 
      totalProjects(0), 
      totalCompleted(0), 
      totalCanceled(0), 
      totalExpired(0) {
    // Initialize with capacity for projects
    projects.reserve(65536);
}

uint64_t ProjectPaymentGateway::createProject(
    id providerWallet,
    uint64_t totalAmount,
    uint64_t deadlineEpoch,
    uint64_t guaranteePeriod,
    uint32_t descriptionLength,
    const std::string& description
) {
    // Validate that the full project amount + fee has been sent
    if (getInvocationReward() < PPG_REGISTRATION_FEE + totalAmount) {
        std::cerr << "Insufficient funds for project creation" << std::endl;
        return 0;
    }
    
    // Validate input parameters
    if (providerWallet == NULL_ID || 
        providerWallet == getCaller() || 
        deadlineEpoch <= getCurrentEpoch() ||
        guaranteePeriod < PPG_MIN_GUARANTEE_PERIOD) {
        std::cerr << "Invalid parameters for project creation" << std::endl;
        return 0;
    }
    
    // Find a free slot for the new project
    int64_t slotIndex = -1;
    for (size_t i = 0; i < projects.size(); i++) {
        if (projects[i].projectId == 0) {
            slotIndex = i;
            break;
        }
    }
    
    // If no slot found, add a new one
    if (slotIndex == -1) {
        if (projects.size() >= 65536) {
            std::cerr << "No available slots for new project" << std::endl;
            return 0;
        }
        slotIndex = projects.size();
        projects.push_back(Project());
    }
    
    // Configure the new project
    Project& newProject = projects[slotIndex];
    newProject.projectId = nextProjectId + 1;
    newProject.clientWallet = getCaller();
    newProject.providerWallet = providerWallet;
    newProject.totalAmount = totalAmount;
    newProject.startEpoch = getCurrentEpoch();
    newProject.deadlineEpoch = deadlineEpoch;
    newProject.guaranteePeriod = guaranteePeriod;
    newProject.descriptionLength = descriptionLength;
    newProject.description = description;
    newProject.status = ProjectStatus::PENDING;
    newProject.clientApproved = false;
    
    // Create a multisig vault for project funds
    std::vector<id> owners = {getCaller(), providerWallet};
    
    // Call MSVAULT contract to create vault
    uint64_t vaultId = callMSVault_registerVault(newProject.projectId, owners, 2, totalAmount);
    
    newProject.vaultId = vaultId;
    
    // Update state
    nextProjectId++;
    totalProjects++;
    
    // Log event
    logEvent(1, newProject.projectId, getCaller(), totalAmount, ProjectStatus::PENDING, vaultId);
    
    return newProject.projectId;
}

bool ProjectPaymentGateway::startProject(uint64_t projectId) {
    Project* project = findProject(projectId);
    
    if (!project) {
        std::cerr << "Project not found: " << projectId << std::endl;
        return false;
    }
    
    bool isProvider = (project->providerWallet == getCaller());
    
    if (!isProvider) {
        std::cerr << "Only the provider can start the project" << std::endl;
        return false;
    }
    
    if (project->status != ProjectStatus::PENDING) {
        std::cerr << "Project must be in PENDING state to start" << std::endl;
        return false;
    }
    
    // Update status
    project->status = ProjectStatus::IN_PROGRESS;
    
    // Log event
    logEvent(1, projectId, getCaller(), 0, ProjectStatus::IN_PROGRESS);
    
    return true;
}

bool ProjectPaymentGateway::approveCompletion(uint64_t projectId) {
    Project* project = findProject(projectId);
    
    if (!project) {
        std::cerr << "Project not found: " << projectId << std::endl;
        return false;
    }
    
    // Verify caller is the client
    if (project->clientWallet != getCaller()) {
        std::cerr << "Only the client can approve completion" << std::endl;
        return false;
    }
    
    if (project->status != ProjectStatus::IN_PROGRESS) {
        std::cerr << "Project must be in IN_PROGRESS state for approval" << std::endl;
        return false;
    }
    
    // Mark the project as completed and approved by the client
    project->status = ProjectStatus::COMPLETED;
    project->clientApproved = true;
    
    // Log event
    logEvent(3, projectId, getCaller(), 0, ProjectStatus::COMPLETED);
    
    return true;
}

bool ProjectPaymentGateway::cancelProject(uint64_t projectId) {
    Project* project = findProject(projectId);
    
    if (!project) {
        std::cerr << "Project not found: " << projectId << std::endl;
        return false;
    }
    
    // Verify caller is the client
    if (project->clientWallet != getCaller()) {
        std::cerr << "Only the client can cancel the project" << std::endl;
        return false;
    }
    
    if (project->status != ProjectStatus::IN_PROGRESS) {
        std::cerr << "Only IN_PROGRESS projects can be canceled" << std::endl;
        return false;
    }
    
    // Calculate funds distribution based on elapsed time
    uint64_t elapsedEpochs = getCurrentEpoch() - project->startEpoch;
    uint64_t totalProjectEpochs = project->deadlineEpoch - project->startEpoch;
    
    // Calculate how much the provider receives (proportional to elapsed time)
    uint64_t providerAmount = 0;
    uint64_t clientRefund = 0;
    
    if (elapsedEpochs >= totalProjectEpochs) {
        // If all time has elapsed, provider receives everything
        providerAmount = project->totalAmount;
        clientRefund = 0;
    } else {
        // Proportion of elapsed time
        providerAmount = (project->totalAmount * elapsedEpochs) / totalProjectEpochs;
        clientRefund = project->totalAmount - providerAmount;
    }
    
    // Distribute funds
    if (providerAmount > 0) {
        callMSVault_releaseTo(project->vaultId, providerAmount, project->providerWallet);
    }
    
    if (clientRefund > 0) {
        callMSVault_releaseTo(project->vaultId, clientRefund, project->clientWallet);
    }
    
    // Update project state
    project->status = ProjectStatus::CANCELED_BY_CLIENT;
    totalCanceled++;
    
    // Log event
    logEvent(2, projectId, getCaller(), 0, ProjectStatus::CANCELED_BY_CLIENT);
    
    return true;
}

bool ProjectPaymentGateway::releaseFunds(uint64_t projectId) {
    Project* project = findProject(projectId);
    
    if (!project) {
        std::cerr << "Project not found: " << projectId << std::endl;
        return false;
    }
    
    // Verify state and guarantee period
    if (project->status != ProjectStatus::COMPLETED || !project->clientApproved) {
        std::cerr << "Project must be completed and approved for fund release" << std::endl;
        return false;
    }
    
    bool guaranteePeriodEnded = getCurrentEpoch() >= (project->deadlineEpoch + project->guaranteePeriod);
    
    if (!guaranteePeriodEnded) {
        std::cerr << "Guarantee period has not ended yet" << std::endl;
        return false;
    }
    
    // Release all funds to the provider
    callMSVault_releaseTo(project->vaultId, project->totalAmount, project->providerWallet);
    
    // Update project state
    project->status = ProjectStatus::FUNDS_RELEASED;
    totalCompleted++;
    
    // Log event
    logEvent(4, projectId, getCaller(), project->totalAmount, ProjectStatus::FUNDS_RELEASED);
    
    return true;
}

ProjectPaymentGateway::ProjectStatusInfo ProjectPaymentGateway::getProjectStatus(uint64_t projectId) {
    ProjectStatusInfo info = {ProjectStatus::PENDING, 0, 0, false};
    
    Project* project = findProject(projectId);
    
    if (project) {
        info.status = project->status;
        info.deadlineEpoch = project->deadlineEpoch;
        info.guaranteeEndEpoch = project->deadlineEpoch + project->guaranteePeriod;
        info.clientApproved = project->clientApproved;
    }
    
    return info;
}

void ProjectPaymentGateway::processEndEpoch() {
    uint64_t vaultFee = MSVAULT_HOLDING_FEE;
    
    for (auto& project : projects) {
        if (project.projectId == 0) {
            continue; // Empty slot
        }
        
        // Maintain active vaults by paying fees
        if (project.status == ProjectStatus::IN_PROGRESS || 
            project.status == ProjectStatus::COMPLETED) {
            
            // Pay fee to keep vault active
            callMSVault_deposit(project.vaultId, vaultFee);
        }
        
        // Check if project expired (passed deadline without completion)
        if (project.status == ProjectStatus::IN_PROGRESS && getCurrentEpoch() > project.deadlineEpoch) {
            // Expired project - return all funds to client
            callMSVault_releaseTo(project.vaultId, project.totalAmount, project.clientWallet);
            
            project.status = ProjectStatus::EXPIRED;
            totalExpired++;
            
            // Log event
            logEvent(5, project.projectId, 0, 0, ProjectStatus::EXPIRED);
        }
        
        // Check if guarantee period ended for completed projects
        else if (project.status == ProjectStatus::COMPLETED && 
                project.clientApproved && 
                getCurrentEpoch() >= (project.deadlineEpoch + project.guaranteePeriod)) {
            
            // Guarantee period ended - release all funds to provider
            callMSVault_releaseTo(project.vaultId, project.totalAmount, project.providerWallet);
            
            project.status = ProjectStatus::FUNDS_RELEASED;
            totalCompleted++;
            
            // Log event
            logEvent(4, project.projectId, 0, project.totalAmount, ProjectStatus::FUNDS_RELEASED);
        }
    }
}

// Private utility functions

Project* ProjectPaymentGateway::findProject(uint64_t projectId) {
    for (auto& project : projects) {
        if (project.projectId == projectId) {
            return &project;
        }
    }
    return nullptr;
}

// Mock functions for contract interaction

uint64_t ProjectPaymentGateway::callMSVault_registerVault(
    uint64_t vaultName, 
    const std::vector<id>& owners, 
    uint8_t requiredApprovals, 
    uint64_t amount
) {
    // In a real implementation, this would call the MSVAULT contract
    std::cout << "Creating vault for project: " << vaultName 
              << " with " << owners.size() << " owners" 
              << " and " << (int)requiredApprovals << " required approvals" 
              << ", initial deposit: " << amount << std::endl;
    
    // Generate a mock vault ID (in real implementation, this would come from MSVAULT)
    return 1000 + vaultName;
}

bool ProjectPaymentGateway::callMSVault_releaseTo(uint64_t vaultId, uint64_t amount, id destination) {
    // In a real implementation, this would call the MSVAULT contract
    std::cout << "Releasing " << amount << " from vault " << vaultId 
              << " to wallet " << destination << std::endl;
    return true;
}

bool ProjectPaymentGateway::callMSVault_deposit(uint64_t vaultId, uint64_t amount) {
    // In a real implementation, this would call the MSVAULT contract
    std::cout << "Depositing " << amount << " to vault " << vaultId << std::endl;
    return true;
}

// System functions

id ProjectPaymentGateway::getCaller() const {
    // In a real implementation, this would get the caller's ID from the blockchain
    // Mock implementation returns a fixed ID
    return 12345;
}

uint64_t ProjectPaymentGateway::getCurrentEpoch() const {
    // In a real implementation, this would get the current epoch from the blockchain
    // Mock implementation uses current time in seconds divided by an epoch length
    const uint64_t EPOCH_LENGTH = 3600; // 1 hour
    return std::time(nullptr) / EPOCH_LENGTH;
}

uint64_t ProjectPaymentGateway::getInvocationReward() const {
    // In a real implementation, this would get the reward amount sent with the transaction
    // Mock implementation returns a large value to pass the checks
    return 10000000ULL;
}

void ProjectPaymentGateway::logEvent(
    uint32_t type, 
    uint64_t projectId, 
    id caller, 
    uint64_t amount, 
    ProjectStatus status,
    uint64_t vaultId
) {
    // In a real implementation, this would log events to the blockchain
    const char* typeStr;
    switch (type) {
        case 1: typeStr = "Project created/started"; break;
        case 2: typeStr = "Project canceled"; break;
        case 3: typeStr = "Project completed"; break;
        case 4: typeStr = "Funds released"; break;
        case 5: typeStr = "Error/Project expired"; break;
        default: typeStr = "Unknown event"; break;
    }
    
    std::cout << "Event: " << typeStr
              << ", Project ID: " << projectId
              << ", Caller: " << caller
              << ", Amount: " << amount
              << ", Status: " << status
              << ", Vault: " << vaultId
              << std::endl;
}