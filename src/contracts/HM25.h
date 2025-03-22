using namespace QPI;

// Constants
static constexpr uint64 MAX_PROJECTS = 100;
static constexpr uint64 MAX_PROJECTS_PER_USER = 50;
static constexpr uint64 EPOCH_FEE = 1; // Fee per epoch to keep the vault active

struct HM25 : public ContractBase
{
public:
    // Project status enum
    enum ProjectStatus {
        Created = 0,
        InProgress = 1,
        Completed = 2,
        Approved = 3,
        Cancelled = 4,
        Expired = 5,
        FundsReleased = 6
    };
    
    // Project data structure
    struct Project {
        id projectId;
        id clientWallet;
        id providerWallet;
        id serviceContract; // Description/terms
        uint64 fundAmount;
        uint64 startEpoch;
        uint64 endEpoch;
        uint64 warrantyPeriodEpochs;
        uint64 completedEpoch;
        uint64 approvedEpoch;
        uint64 cancelledEpoch;
        uint8 status; // ProjectStatus
    };
    
    // Input/output structs for functions
    struct CreateProject_input {
        id providerWallet;
        id serviceContract;
        uint64 timeframeEpochs;
        uint64 warrantyPeriodEpochs;
    };
    struct CreateProject_output {};
    
    struct CompleteProject_input {
        id projectId;
    };
    struct CompleteProject_output {};
    
    struct ApproveProject_input {
        id projectId;
    };
    struct ApproveProject_output {};
    
    struct CancelProject_input {
        id projectId;
    };
    struct CancelProject_output {};
    
    struct WithdrawFunds_input {
        id projectId;
    };
    struct WithdrawFunds_output {};
    
    struct GetProjectDetails_input {
        id projectId;
    };
    struct GetProjectDetails_output {
        Project project;
        bit exists;
    };
    
    struct GetClientProjects_input {};
    struct GetClientProjects_output {
        uint64 count;
        Array<id, MAX_PROJECTS_PER_USER> projectIds;
    };
    
    struct GetProviderProjects_input {};
    struct GetProviderProjects_output {
        uint64 count;
        Array<id, MAX_PROJECTS_PER_USER> projectIds;
    };
    
    struct GetStats_input {};
    struct GetStats_output {
        uint64 totalProjects;
        uint64 activeProjects;
        uint64 completedProjects;
        uint64 cancelledProjects;
        uint64 expiredProjects;
    };
    
    struct END_EPOCH_locals {
        uint64 currentEpoch;
        uint64 i;
        Project project;
        uint64 elapsedEpochs;
        uint64 totalEpochs;
        uint64 percentComplete;
        uint64 providerAmount;
        uint64 clientAmount;
    };

private:
    // State variables
    Array<Project, MAX_PROJECTS> projects;
    uint64 projectCount;
    uint64 activeProjects;
    uint64 completedProjects;
    uint64 cancelledProjects;
    uint64 expiredProjects;
    
    // Client and provider project tracking
    Array<id, MAX_PROJECTS_PER_USER> clientProjects[MAX_PROJECTS_PER_USER];
    Array<uint64, MAX_PROJECTS_PER_USER> clientProjectCounts;
    Array<id, MAX_PROJECTS_PER_USER> providerProjects[MAX_PROJECTS_PER_USER];
    Array<uint64, MAX_PROJECTS_PER_USER> providerProjectCounts;
    
    // Helper methods
    PRIVATE_FUNCTION(findProject)
        output.exists = false;
        output.index = 0;
        
        for (uint64 i = 0; i < state.projectCount; i++) {
            if (state.projects.get(i).projectId == input.projectId) {
                output.exists = true;
                output.index = i;
                break;
            }
        }
    _
    
    PRIVATE_FUNCTION(getClientIndex)
        output.exists = false;
        output.index = 0;
        
        for (uint64 i = 0; i < MAX_PROJECTS_PER_USER; i++) {
            if (input.wallet == state.clientProjects[i].get(0)) {
                output.exists = true;
                output.index = i;
                break;
            }
        }
    _
    
    PRIVATE_FUNCTION(getProviderIndex)
        output.exists = false;
        output.index = 0;
        
        for (uint64 i = 0; i < MAX_PROJECTS_PER_USER; i++) {
            if (input.wallet == state.providerProjects[i].get(0)) {
                output.exists = true;
                output.index = i;
                break;
            }
        }
    _
    
    // Main functions
    PUBLIC_PROCEDURE(CreateProject)
        // Verify there's funding provided
        if (qpi.invocationReward() == 0) {
            return;
        }
        
        // Create project with unique ID (using combination of client, provider, and timestamp)
        id projectId = hashId(qpi.invocator(), input.providerWallet, qpi.epoch());
        
        // Create new project
        Project newProject;
        newProject.projectId = projectId;
        newProject.clientWallet = qpi.invocator();
        newProject.providerWallet = input.providerWallet;
        newProject.serviceContract = input.serviceContract;
        newProject.fundAmount = qpi.invocationReward();
        newProject.startEpoch = qpi.epoch();
        newProject.endEpoch = qpi.epoch() + input.timeframeEpochs;
        newProject.warrantyPeriodEpochs = input.warrantyPeriodEpochs;
        newProject.completedEpoch = 0;
        newProject.approvedEpoch = 0;
        newProject.cancelledEpoch = 0;
        newProject.status = ProjectStatus::Created;
        
        // Add project to state
        state.projects.set(state.projectCount, newProject);
        
        // Update client projects
        findClientIndex_input ci_input;
        ci_input.wallet = qpi.invocator();
        findClientIndex_output ci_output;
        findClientIndex(qpi, state, ci_input, ci_output);
        
        uint64 clientIndex = ci_output.index;
        uint64 clientCount = state.clientProjectCounts.get(clientIndex);
        state.clientProjects[clientIndex].set(clientCount, projectId);
        state.clientProjectCounts.set(clientIndex, clientCount + 1);
        
        // Update provider projects
        findProviderIndex_input pi_input;
        pi_input.wallet = input.providerWallet;
        findProviderIndex_output pi_output;
        findProviderIndex(qpi, state, pi_input, pi_output);
        
        uint64 providerIndex = pi_output.index;
        uint64 providerCount = state.providerProjectCounts.get(providerIndex);
        state.providerProjects[providerIndex].set(providerCount, projectId);
        state.providerProjectCounts.set(providerIndex, providerCount + 1);
        
        // Update counters
        state.projectCount++;
        state.activeProjects++;
    _
    
    PUBLIC_PROCEDURE(CompleteProject)
        // Find project
        findProject_input fp_input;
        fp_input.projectId = input.projectId;
        findProject_output fp_output;
        findProject(qpi, state, fp_input, fp_output);
        
        if (!fp_output.exists) {
            return;
        }
        
        uint64 projectIndex = fp_output.index;
        Project project = state.projects.get(projectIndex);
        
        // Verify caller is the provider
        if (qpi.invocator() != project.providerWallet) {
            return;
        }
        
        // Verify project is in progress
        if (project.status != ProjectStatus::Created && project.status != ProjectStatus::InProgress) {
            return;
        }
        
        // Mark as completed
        project.status = ProjectStatus::Completed;
        project.completedEpoch = qpi.epoch();
        
        // Update state
        state.projects.set(projectIndex, project);
    _
    
    PUBLIC_PROCEDURE(ApproveProject)
        // Find project
        findProject_input fp_input;
        fp_input.projectId = input.projectId;
        findProject_output fp_output;
        findProject(qpi, state, fp_input, fp_output);
        
        if (!fp_output.exists) {
            return;
        }
        
        uint64 projectIndex = fp_output.index;
        Project project = state.projects.get(projectIndex);
        
        // Verify caller is the client
        if (qpi.invocator() != project.clientWallet) {
            return;
        }
        
        // Verify project is completed
        if (project.status != ProjectStatus::Completed) {
            return;
        }
        
        // Mark as approved
        project.status = ProjectStatus::Approved;
        project.approvedEpoch = qpi.epoch();
        
        // Update state
        state.projects.set(projectIndex, project);
    _
    
    PUBLIC_PROCEDURE(CancelProject)
        // Find project
        findProject_input fp_input;
        fp_input.projectId = input.projectId;
        findProject_output fp_output;
        findProject(qpi, state, fp_input, fp_output);
        
        if (!fp_output.exists) {
            return;
        }
        
        uint64 projectIndex = fp_output.index;
        Project project = state.projects.get(projectIndex);
        
        // Verify caller is the client
        if (qpi.invocator() != project.clientWallet) {
            return;
        }
        
        // Verify project can be cancelled (not already completed or cancelled)
        if (project.status != ProjectStatus::Created && project.status != ProjectStatus::InProgress) {
            return;
        }
        
        // Mark as cancelled
        project.status = ProjectStatus::Cancelled;
        project.cancelledEpoch = qpi.epoch();
        
        // Calculate provider's partial payment based on elapsed time
        uint64 elapsedEpochs = qpi.epoch() - project.startEpoch;
        uint64 totalEpochs = project.endEpoch - project.startEpoch;
        
        // Ensure we don't divide by zero
        if (totalEpochs == 0) {
            totalEpochs = 1;
        }
        
        // Calculate percentage (0-100)
        uint64 percentComplete = (elapsedEpochs * 100) / totalEpochs;
        if (percentComplete > 100) {
            percentComplete = 100;
        }
        
        // Calculate payment amounts
        uint64 providerAmount = (project.fundAmount * percentComplete) / 100;
        uint64 clientAmount = project.fundAmount - providerAmount;
        
        // Transfer funds
        if (providerAmount > 0) {
            qpi.transfer(project.providerWallet, providerAmount);
        }
        
        if (clientAmount > 0) {
            qpi.transfer(project.clientWallet, clientAmount);
        }
        
        // Update state
        state.projects.set(projectIndex, project);
        state.activeProjects--;
        state.cancelledProjects++;
    _
    
    PUBLIC_PROCEDURE(WithdrawFunds)
        // Find project
        findProject_input fp_input;
        fp_input.projectId = input.projectId;
        findProject_output fp_output;
        findProject(qpi, state, fp_input, fp_output);
        
        if (!fp_output.exists) {
            return;
        }
        
        uint64 projectIndex = fp_output.index;
        Project project = state.projects.get(projectIndex);
        
        // Verify caller is the provider
        if (qpi.invocator() != project.providerWallet) {
            return;
        }
        
        // Verify project is approved
        if (project.status != ProjectStatus::Approved) {
            return;
        }
        
        // Verify warranty period has passed
        if (qpi.epoch() < project.approvedEpoch + project.warrantyPeriodEpochs) {
            return;
        }
        
        // Transfer funds to provider
        qpi.transfer(project.providerWallet, project.fundAmount);
        
        // Update project status
        project.status = ProjectStatus::FundsReleased;
        state.projects.set(projectIndex, project);
        
        // Update counters
        state.activeProjects--;
        state.completedProjects++;
    _
    
    PUBLIC_FUNCTION(GetProjectDetails)
        findProject_input fp_input;
        fp_input.projectId = input.projectId;
        findProject_output fp_output;
        findProject(qpi, state, fp_input, fp_output);
        
        output.exists = fp_output.exists;
        if (output.exists) {
            output.project = state.projects.get(fp_output.index);
        }
    _
    
    PUBLIC_FUNCTION(GetClientProjects)
        findClientIndex_input ci_input;
        ci_input.wallet = qpi.invocator();
        findClientIndex_output ci_output;
        findClientIndex(qpi, state, ci_input, ci_output);
        
        output.count = 0;
        if (ci_output.exists) {
            uint64 clientIndex = ci_output.index;
            output.count = state.clientProjectCounts.get(clientIndex);
            
            for (uint64 i = 0; i < output.count; i++) {
                output.projectIds.set(i, state.clientProjects[clientIndex].get(i));
            }
        }
    _
    
    PUBLIC_FUNCTION(GetProviderProjects)
        findProviderIndex_input pi_input;
        pi_input.wallet = qpi.invocator();
        findProviderIndex_output pi_output;
        findProviderIndex(qpi, state, pi_input, pi_output);
        
        output.count = 0;
        if (pi_output.exists) {
            uint64 providerIndex = pi_output.index;
            output.count = state.providerProjectCounts.get(providerIndex);
            
            for (uint64 i = 0; i < output.count; i++) {
                output.projectIds.set(i, state.providerProjects[providerIndex].get(i));
            }
        }
    _
    
    PUBLIC_FUNCTION(GetStats)
        output.totalProjects = state.projectCount;
        output.activeProjects = state.activeProjects;
        output.completedProjects = state.completedProjects;
        output.cancelledProjects = state.cancelledProjects;
        output.expiredProjects = state.expiredProjects;
    _
    
    END_EPOCH_WITH_LOCALS
        locals.currentEpoch = qpi.epoch();
        
        // Process all active projects
        for (locals.i = 0; locals.i < state.projectCount; locals.i++) {
            locals.project = state.projects.get(locals.i);
            
            // Handle expired projects
            if (locals.project.status == ProjectStatus::Created || locals.project.status == ProjectStatus::InProgress) {
                if (locals.currentEpoch > locals.project.endEpoch) {
                    // Project expired, return funds to client
                    locals.project.status = ProjectStatus::Expired;
                    qpi.transfer(locals.project.clientWallet, locals.project.fundAmount);
                    
                    // Update state
                    state.projects.set(locals.i, locals.project);
                    state.activeProjects--;
                    state.expiredProjects++;
                } else {
                    // Pay epoch fee to keep vault active
                    if (locals.project.fundAmount > EPOCH_FEE) {
                        locals.project.fundAmount -= EPOCH_FEE;
                        qpi.burn(EPOCH_FEE); // Burn the fee
                        state.projects.set(locals.i, locals.project);
                    }
                }
            }
        }
    _
    
    // Function registrations
    REGISTER_USER_FUNCTIONS_AND_PROCEDURES
        REGISTER_USER_PROCEDURE(CreateProject, 1);
        REGISTER_USER_PROCEDURE(CompleteProject, 2);
        REGISTER_USER_PROCEDURE(ApproveProject, 3);
        REGISTER_USER_PROCEDURE(CancelProject, 4);
        REGISTER_USER_PROCEDURE(WithdrawFunds, 5);
        
        REGISTER_USER_FUNCTION(GetProjectDetails, 1);
        REGISTER_USER_FUNCTION(GetClientProjects, 2);
        REGISTER_USER_FUNCTION(GetProviderProjects, 3);
        REGISTER_USER_FUNCTION(GetStats, 4);
    _
    
    INITIALIZE
        state.projectCount = 0;
        state.activeProjects = 0;
        state.completedProjects = 0;
        state.cancelledProjects = 0;
        state.expiredProjects = 0;
        
        // Initialize client and provider tracking arrays
        for (uint64 i = 0; i < MAX_PROJECTS_PER_USER; i++) {
            state.clientProjectCounts.set(i, 0);
            state.providerProjectCounts.set(i, 0);
        }
    _
};
