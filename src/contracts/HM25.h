using namespace QPI;

// Constants
constexpr uint64 HM25_MAX_PROJECTS = 4096;
constexpr uint64 HM25_MIN_DEPOSIT_AMOUNT = 1000000; // Minimum deposit: 1 QU
constexpr uint64 HM25_MAINTENANCE_FEE = 100000; // Fee per epoch to maintain active project
constexpr uint64 HM25_CREATION_FEE = 1000000; // Fee to create a project
constexpr uint64 HM25_DURATION_MIN_EPOCHS = 1; // Minimum project duration
constexpr uint64 HM25_DURATION_MAX_EPOCHS = 52; // Maximum project duration (1 year)
constexpr uint64 HM25_WARRANTY_MIN_EPOCHS = 1; // Minimum warranty period
constexpr uint64 HM25_WARRANTY_MAX_EPOCHS = 12; // Maximum warranty period

// Project status
constexpr uint8 HM25_STATUS_CREATED = 0;    // Project created but not funded
constexpr uint8 HM25_STATUS_FUNDED = 1;     // Project funded, in progress
constexpr uint8 HM25_STATUS_DELIVERED = 2;  // Project delivered, in warranty period
constexpr uint8 HM25_STATUS_COMPLETED = 3;  // Project completed, funds released
constexpr uint8 HM25_STATUS_CANCELED = 4;   // Project canceled, partial funds distributed
constexpr uint8 HM25_STATUS_REFUNDED = 5;   // Project ended, funds returned to client
constexpr uint8 HM25_STATUS_INACTIVE = 6;   // Project inactive (due to lack of maintenance fee)

// Log codes
enum HM25LogType {
    LogProjectCreated = 0,
    LogProjectFunded = 1,
    LogProjectDelivered = 2,
    LogProjectCompleted = 3,
    LogProjectCanceled = 4,
    LogProjectRefunded = 5,
    LogProjectInactive = 6,
    LogInsufficientFunds = 7,
    LogUnauthorized = 8,
    LogInvalidState = 9,
    LogMaintenancePaid = 10
};

struct HM25Logger {
    uint32 _contractIndex;
    uint32 _type;
    uint64 projectId;
    id clientId;
    id providerId;
    uint64 amount;
    char _terminator;
};

struct HM25
{
};

struct HM25 : public ContractBase
{
public:
    // Project data structure
    struct Project {
        id clientId;                  // Client wallet address
        id providerId;                // Provider wallet address
        id contractDescription;       // Description or hash of the contract
        uint64 totalAmount;           // Total project amount
        uint64 remainingAmount;       // Remaining amount (after potential partial releases or fees)
        uint32 creationEpoch;         // When the project was created
        uint32 deadlineEpoch;         // Project must be delivered by this epoch
        uint32 warrantyEndEpoch;      // End of warranty period
        uint32 lastMaintenanceEpoch;  // Last epoch when maintenance fee was paid
        uint8 status;                 // Project status
        bit isActive;                 // Is the project active
    };

    // Input and output structures for procedures and functions
    struct createProject_input {
        id clientId;              // Client wallet
        id providerId;            // Provider wallet
        id contractDescription;   // Project description/contract
        uint32 durationEpochs;    // Duration in epochs
        uint32 warrantyEpochs;    // Warranty period in epochs
    };
    struct createProject_output {
        uint64 projectId;         // Created project ID or 0 if failed
    };

    struct fundProject_input {
        uint64 projectId;         // Project ID to fund
    };
    struct fundProject_output {
        uint8 result;             // 1: success, 0: failed
    };

    struct deliverProject_input {
        uint64 projectId;         // Project ID to mark as delivered
    };
    struct deliverProject_output {
        uint8 result;             // 1: success, 0: failed
    };

    struct approveProject_input {
        uint64 projectId;         // Project ID to approve
    };
    struct approveProject_output {
        uint8 result;             // 1: success, 0: failed
    };

    struct cancelProject_input {
        uint64 projectId;         // Project ID to cancel
    };
    struct cancelProject_output {
        uint8 result;             // 1: success, 0: failed
    };

    struct refundProject_input {
        uint64 projectId;         // Project ID to refund
    };
    struct refundProject_output {
        uint8 result;             // 1: success, 0: failed
    };

    struct payMaintenance_input {
        uint64 projectId;         // Project ID to pay maintenance for
    };
    struct payMaintenance_output {
        uint8 result;             // 1: success, 0: failed
    };

    struct getProjectInfo_input {
        uint64 projectId;         // Project ID to query
    };
    struct getProjectInfo_output {
        uint8 status;             // Project status
        id clientId;              // Client wallet
        id providerId;            // Provider wallet
        id contractDescription;   // Project description
        uint64 totalAmount;       // Total project amount
        uint64 remainingAmount;   // Remaining funds
        uint32 creationEpoch;     // Creation epoch
        uint32 deadlineEpoch;     // Deadline epoch
        uint32 warrantyEndEpoch;  // Warranty end epoch
        uint32 lastMaintenanceEpoch; // Last maintenance epoch
        bit isActive;             // Is project active
        uint64 partialPaymentAmount; // Current partial payment if canceled now
    };

    struct getClientProjects_input {
        id clientId;              // Client wallet to query projects for
    };
    struct getClientProjects_output {
        uint64 count;             // Number of projects
        Array<uint64, HM25_MAX_PROJECTS> projectIds; // Project IDs
    };

    struct getProviderProjects_input {
        id providerId;            // Provider wallet to query projects for
    };
    struct getProviderProjects_output {
        uint64 count;             // Number of projects
        Array<uint64, HM25_MAX_PROJECTS> projectIds; // Project IDs
    };

    struct calculatePartialPayment_input {
        uint64 projectId;         // Project ID to calculate payment for
    };
    struct calculatePartialPayment_output {
        uint64 clientAmount;      // Amount to return to client
        uint64 providerAmount;    // Amount to pay to provider
    };

protected:
    // Contract state
    Array<Project, HM25_MAX_PROJECTS> projects;
    uint64 nextProjectId;
    uint64 totalFeesCollected;
    uint64 totalFeesPaid;

    // Local structures for procedure implementation
    struct createProject_locals {
        Project newProject;
        uint64 projectId;
        HM25Logger log;
    };

    struct fundProject_locals {
        Project project;
        HM25Logger log;
    };

    struct deliverProject_locals {
        Project project;
        HM25Logger log;
    };

    struct approveProject_locals {
        Project project;
        HM25Logger log;
    };

    struct cancelProject_locals {
        Project project;
        uint64 clientAmount;
        uint64 providerAmount;
        HM25Logger log;
        calculatePartialPayment_input calc_input;
        calculatePartialPayment_output calc_output;
    };

    struct refundProject_locals {
        Project project;
        HM25Logger log;
    };

    struct payMaintenance_locals {
        Project project;
        HM25Logger log;
    };

    struct getProjectInfo_locals {
        calculatePartialPayment_input calc_input;
        calculatePartialPayment_output calc_output;
    };

    struct getClientProjects_locals {
        uint64 count;
    };

    struct getProviderProjects_locals {
        uint64 count;
    };

    struct calculatePartialPayment_locals {
        Project project;
        uint64 totalEpochs;
        uint64 elapsedEpochs;
        uint64 percentComplete;
    };

    struct END_EPOCH_locals {
        uint64 i;
        Project project;
        uint64 amountToDistribute;
    };

    // Helper methods for calculating partial payments based on time passed
    PRIVATE_FUNCTION_WITH_LOCALS(calculatePartialPayment)
        locals.project = state.projects.get(input.projectId);
        
        if (!locals.project.isActive || locals.project.status != HM25_STATUS_FUNDED) {
            output.clientAmount = 0;
            output.providerAmount = 0;
            return;
        }
        
        // Calculate percentage of project completed based on time elapsed
        locals.totalEpochs = locals.project.deadlineEpoch - locals.project.creationEpoch;
        locals.elapsedEpochs = qpi.epoch() - locals.project.creationEpoch;
        
        // Cap elapsed epochs to total epochs
        if (locals.elapsedEpochs > locals.totalEpochs) {
            locals.elapsedEpochs = locals.totalEpochs;
        }
        
        // Calculate percentage completed (0-100)
        locals.percentComplete = (locals.elapsedEpochs * 100) / locals.totalEpochs;
        
        // Calculate amounts
        output.providerAmount = (locals.project.remainingAmount * locals.percentComplete) / 100;
        output.clientAmount = locals.project.remainingAmount - output.providerAmount;
    _

    // Main functionality procedures
    PUBLIC_PROCEDURE_WITH_LOCALS(createProject)
        // Validate inputs
        if (qpi.invocationReward() < HM25_CREATION_FEE) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        
        if (input.clientId == NULL_ID || input.providerId == NULL_ID || 
            input.durationEpochs < HM25_DURATION_MIN_EPOCHS || 
            input.durationEpochs > HM25_DURATION_MAX_EPOCHS ||
            input.warrantyEpochs < HM25_WARRANTY_MIN_EPOCHS ||
            input.warrantyEpochs > HM25_WARRANTY_MAX_EPOCHS) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        
        // Find an available slot for the project
        locals.projectId = state.nextProjectId;
        
        // Initialize project
        locals.newProject.clientId = input.clientId;
        locals.newProject.providerId = input.providerId;
        locals.newProject.contractDescription = input.contractDescription;
        locals.newProject.totalAmount = 0; // Will be set when funded
        locals.newProject.remainingAmount = 0;
        locals.newProject.creationEpoch = qpi.epoch();
        locals.newProject.deadlineEpoch = qpi.epoch() + input.durationEpochs;
        locals.newProject.warrantyEndEpoch = qpi.epoch() + input.durationEpochs + input.warrantyEpochs;
        locals.newProject.lastMaintenanceEpoch = qpi.epoch();
        locals.newProject.status = HM25_STATUS_CREATED;
        locals.newProject.isActive = true;
        
        // Store the project
        state.projects.set(locals.projectId, locals.newProject);
        state.nextProjectId++;
        
        // Return remaining funds
        if (qpi.invocationReward() > HM25_CREATION_FEE) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward() - HM25_CREATION_FEE);
        }
        
        state.totalFeesCollected += HM25_CREATION_FEE;
        
        // Log event
        locals.log._contractIndex = HM25_CONTRACT_INDEX;
        locals.log._type = LogProjectCreated;
        locals.log.projectId = locals.projectId;
        locals.log.clientId = input.clientId;
        locals.log.providerId = input.providerId;
        locals.log.amount = 0;
        LOG_INFO(locals.log);
        
        // Return created project ID
        output.projectId = locals.projectId;
    _

    PUBLIC_PROCEDURE_WITH_LOCALS(fundProject)
        if (qpi.invocationReward() < HM25_MIN_DEPOSIT_AMOUNT) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInsufficientFunds;
            locals.log.projectId = input.projectId;
            locals.log.amount = qpi.invocationReward();
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        locals.project = state.projects.get(input.projectId);
        
        // Verify project exists and is in correct state
        if (!locals.project.isActive || locals.project.status != HM25_STATUS_CREATED) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInvalidState;
            locals.log.projectId = input.projectId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Verify caller is the client
        if (qpi.invocator() != locals.project.clientId) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogUnauthorized;
            locals.log.projectId = input.projectId;
            locals.log.clientId = locals.project.clientId;
            locals.log.providerId = locals.project.providerId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Update project state
        locals.project.totalAmount = qpi.invocationReward();
        locals.project.remainingAmount = qpi.invocationReward();
        locals.project.status = HM25_STATUS_FUNDED;
        
        // Save updated project
        state.projects.set(input.projectId, locals.project);
        
        // Log event
        locals.log._contractIndex = HM25_CONTRACT_INDEX;
        locals.log._type = LogProjectFunded;
        locals.log.projectId = input.projectId;
        locals.log.clientId = locals.project.clientId;
        locals.log.providerId = locals.project.providerId;
        locals.log.amount = qpi.invocationReward();
        LOG_INFO(locals.log);
        
        output.result = 1;
    _

    PUBLIC_PROCEDURE_WITH_LOCALS(deliverProject)
        locals.project = state.projects.get(input.projectId);
        
        // Verify project exists and is in correct state
        if (!locals.project.isActive || locals.project.status != HM25_STATUS_FUNDED) {
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInvalidState;
            locals.log.projectId = input.projectId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Verify caller is the provider
        if (qpi.invocator() != locals.project.providerId) {
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogUnauthorized;
            locals.log.projectId = input.projectId;
            locals.log.clientId = locals.project.clientId;
            locals.log.providerId = locals.project.providerId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Return funds if invocation reward sent
        if (qpi.invocationReward() > 0) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
        }
        
        // Check if past deadline
        if (qpi.epoch() > locals.project.deadlineEpoch) {
            // Project past deadline, don't allow delivery
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInvalidState;
            locals.log.projectId = input.projectId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Update project state
        locals.project.status = HM25_STATUS_DELIVERED;
        
        // Save updated project
        state.projects.set(input.projectId, locals.project);
        
        // Log event
        locals.log._contractIndex = HM25_CONTRACT_INDEX;
        locals.log._type = LogProjectDelivered;
        locals.log.projectId = input.projectId;
        locals.log.clientId = locals.project.clientId;
        locals.log.providerId = locals.project.providerId;
        LOG_INFO(locals.log);
        
        output.result = 1;
    _

    PUBLIC_PROCEDURE_WITH_LOCALS(approveProject)
        locals.project = state.projects.get(input.projectId);
        
        // Verify project exists and is in correct state
        if (!locals.project.isActive || locals.project.status != HM25_STATUS_DELIVERED) {
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInvalidState;
            locals.log.projectId = input.projectId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Verify caller is the client
        if (qpi.invocator() != locals.project.clientId) {
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogUnauthorized;
            locals.log.projectId = input.projectId;
            locals.log.clientId = locals.project.clientId;
            locals.log.providerId = locals.project.providerId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Return funds if invocation reward sent
        if (qpi.invocationReward() > 0) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
        }
        
        // Check if we need to wait for warranty period
        if (qpi.epoch() >= locals.project.warrantyEndEpoch) {
            // Warranty period ended, release funds and complete project
            
            // Transfer remaining funds to provider
            qpi.transfer(locals.project.providerId, locals.project.remainingAmount);
            
            // Update project status
            locals.project.status = HM25_STATUS_COMPLETED;
            locals.project.remainingAmount = 0;
            
            // Save updated project
            state.projects.set(input.projectId, locals.project);
            
            // Log event
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogProjectCompleted;
            locals.log.projectId = input.projectId;
            locals.log.clientId = locals.project.clientId;
            locals.log.providerId = locals.project.providerId;
            locals.log.amount = locals.project.remainingAmount;
            LOG_INFO(locals.log);
        } else {
            // Still in warranty period, just update status to wait for warranty period
            locals.project.status = HM25_STATUS_DELIVERED; 
            
            // Save updated project
            state.projects.set(input.projectId, locals.project);
        }
        
        output.result = 1;
    _

    PUBLIC_PROCEDURE_WITH_LOCALS(cancelProject)
        locals.project = state.projects.get(input.projectId);
        
        // Verify project exists and is in correct state
        if (!locals.project.isActive || 
            (locals.project.status != HM25_STATUS_FUNDED && 
             locals.project.status != HM25_STATUS_DELIVERED)) {
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInvalidState;
            locals.log.projectId = input.projectId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Verify caller is the client
        if (qpi.invocator() != locals.project.clientId) {
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogUnauthorized;
            locals.log.projectId = input.projectId;
            locals.log.clientId = locals.project.clientId;
            locals.log.providerId = locals.project.providerId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Return funds if invocation reward sent
        if (qpi.invocationReward() > 0) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
        }
        
        // Calculate partial payments
        locals.calc_input.projectId = input.projectId;
        calculatePartialPayment(qpi, state, locals.calc_input, locals.calc_output, locals.calculatePartialPayment_locals);
        locals.clientAmount = locals.calc_output.clientAmount;
        locals.providerAmount = locals.calc_output.providerAmount;
        
        // Distribute funds
        if (locals.clientAmount > 0) {
            qpi.transfer(locals.project.clientId, locals.clientAmount);
        }
        
        if (locals.providerAmount > 0) {
            qpi.transfer(locals.project.providerId, locals.providerAmount);
        }
        
        // Update project state
        locals.project.status = HM25_STATUS_CANCELED;
        locals.project.remainingAmount = 0;
        
        // Save updated project
        state.projects.set(input.projectId, locals.project);
        
        // Log event
        locals.log._contractIndex = HM25_CONTRACT_INDEX;
        locals.log._type = LogProjectCanceled;
        locals.log.projectId = input.projectId;
        locals.log.clientId = locals.project.clientId;
        locals.log.providerId = locals.project.providerId;
        locals.log.amount = locals.providerAmount;
        LOG_INFO(locals.log);
        
        output.result = 1;
    _

    PUBLIC_PROCEDURE_WITH_LOCALS(refundProject)
        locals.project = state.projects.get(input.projectId);
        
        // Verify project exists and is in FUNDED state
        if (!locals.project.isActive || locals.project.status != HM25_STATUS_FUNDED) {
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInvalidState;
            locals.log.projectId = input.projectId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Verify past deadline and caller is the client
        if (qpi.epoch() <= locals.project.deadlineEpoch || qpi.invocator() != locals.project.clientId) {
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogUnauthorized;
            locals.log.projectId = input.projectId;
            locals.log.clientId = locals.project.clientId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Return funds if invocation reward sent
        if (qpi.invocationReward() > 0) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
        }
        
        // Return all remaining funds to client due to project not being delivered on time
        qpi.transfer(locals.project.clientId, locals.project.remainingAmount);
        
        // Update project state
        locals.project.status = HM25_STATUS_REFUNDED;
        locals.project.remainingAmount = 0;
        
        // Save updated project
        state.projects.set(input.projectId, locals.project);
        
        // Log event
        locals.log._contractIndex = HM25_CONTRACT_INDEX;
        locals.log._type = LogProjectRefunded;
        locals.log.projectId = input.projectId;
        locals.log.clientId = locals.project.clientId;
        locals.log.amount = locals.project.remainingAmount;
        LOG_INFO(locals.log);
        
        output.result = 1;
    _

    PUBLIC_PROCEDURE_WITH_LOCALS(payMaintenance)
        if (qpi.invocationReward() < HM25_MAINTENANCE_FEE) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInsufficientFunds;
            locals.log.projectId = input.projectId;
            locals.log.amount = qpi.invocationReward();
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        locals.project = state.projects.get(input.projectId);
        
        // Verify project exists and is in active state with money remaining
        if (!locals.project.isActive || 
            locals.project.status == HM25_STATUS_COMPLETED || 
            locals.project.status == HM25_STATUS_CANCELED ||
            locals.project.status == HM25_STATUS_REFUNDED ||
            locals.project.status == HM25_STATUS_INACTIVE ||
            locals.project.remainingAmount == 0) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogInvalidState;
            locals.log.projectId = input.projectId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Verify caller is the client or provider
        if (qpi.invocator() != locals.project.clientId && qpi.invocator() != locals.project.providerId) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            locals.log._contractIndex = HM25_CONTRACT_INDEX;
            locals.log._type = LogUnauthorized;
            locals.log.projectId = input.projectId;
            LOG_INFO(locals.log);
            output.result = 0;
            return;
        }
        
        // Return excess funds
        if (qpi.invocationReward() > HM25_MAINTENANCE_FEE) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward() - HM25_MAINTENANCE_FEE);
        }
        
        // Update maintenance epoch
        locals.project.lastMaintenanceEpoch = qpi.epoch();
        
        // Update fees collected
        state.totalFeesCollected += HM25_MAINTENANCE_FEE;
        
        // Save updated project
        state.projects.set(input.projectId, locals.project);
        
        // Log event
        locals.log._contractIndex = HM25_CONTRACT_INDEX;
        locals.log._type = LogMaintenancePaid;
        locals.log.projectId = input.projectId;
        locals.log.amount = HM25_MAINTENANCE_FEE;
        LOG_INFO(locals.log);
        
        output.result = 1;
    _

    // Query functions
    PUBLIC_FUNCTION_WITH_LOCALS(getProjectInfo)
        output.status = HM25_STATUS_INACTIVE;
        output.clientId = NULL_ID;
        output.providerId = NULL_ID;
        output.contractDescription = NULL_ID;
        output.totalAmount = 0;
        output.remainingAmount = 0;
        output.creationEpoch = 0;
        output.deadlineEpoch = 0;
        output.warrantyEndEpoch = 0;
        output.lastMaintenanceEpoch = 0;
        output.isActive = false;
        output.partialPaymentAmount = 0;
        
        if (input.projectId >= state.nextProjectId) {
            return;
        }
        
        Project project = state.projects.get(input.projectId);
        
        if (!project.isActive) {
            return;
        }
        
        output.status = project.status;
        output.clientId = project.clientId;
        output.providerId = project.providerId;
        output.contractDescription = project.contractDescription;
        output.totalAmount = project.totalAmount;
        output.remainingAmount = project.remainingAmount;
        output.creationEpoch = project.creationEpoch;
        output.deadlineEpoch = project.deadlineEpoch;
        output.warrantyEndEpoch = project.warrantyEndEpoch;
        output.lastMaintenanceEpoch = project.lastMaintenanceEpoch;
        output.isActive = project.isActive;
        
        // Calculate partial payment if in funded state
        if (project.status == HM25_STATUS_FUNDED) {
            locals.calc_input.projectId = input.projectId;
            calculatePartialPayment(qpi, state, locals.calc_input, locals.calc_output, locals.calculatePartialPayment_locals);
            output.partialPaymentAmount = locals.calc_output.providerAmount;
        }
    _

    PUBLIC_FUNCTION_WITH_LOCALS(getClientProjects)
        output.count = 0;
        
        for (locals.count = 0; locals.count < state.nextProjectId && locals.count < HM25_MAX_PROJECTS; locals.count++) {
            Project project = state.projects.get(locals.count);
            if (project.isActive && project.clientId == input.clientId) {
                output.projectIds.set(output.count, locals.count);
                output.count++;
            }
        }
    _

    PUBLIC_FUNCTION_WITH_LOCALS(getProviderProjects)
        output.count = 0;
        
        for (locals.count = 0; locals.count < state.nextProjectId && locals.count < HM25_MAX_PROJECTS; locals.count++) {
            Project project = state.projects.get(locals.count);
            if (project.isActive && project.providerId == input.providerId) {
                output.projectIds.set(output.count, locals.count);
                output.count++;
            }
        }
    _

    // System procedures
    INITIALIZE
        state.nextProjectId = 0;
        state.totalFeesCollected = 0;
        state.totalFeesPaid = 0;
    _

    END_EPOCH_WITH_LOCALS
        // Check maintenance and distribute fees
        for (locals.i = 0; locals.i < state.nextProjectId; locals.i++) {
            locals.project = state.projects.get(locals.i);
            
            if (locals.project.isActive && 
                (locals.project.status == HM25_STATUS_FUNDED || 
                 locals.project.status == HM25_STATUS_DELIVERED)) {
                
                // Check if maintenance fee is needed
                if (qpi.epoch() > locals.project.lastMaintenanceEpoch + 1) {
                    // More than one epoch passed since last maintenance payment
                    
                    // Set to inactive if no funds to cover maintenance fee
                    if (locals.project.remainingAmount <= HM25_MAINTENANCE_FEE) {
                        locals.project.isActive = false;
                        locals.project.status = HM25_STATUS_INACTIVE;
                        
                        // Log event
                        HM25Logger log;
                        log._contractIndex = HM25_CONTRACT_INDEX;
                        log._type = LogProjectInactive;
                        log.projectId = locals.i;
                        LOG_INFO(log);
                    } else {
                        // Deduct maintenance fee from project funds
                        locals.project.remainingAmount -= HM25_MAINTENANCE_FEE;
                        locals.project.lastMaintenanceEpoch = qpi.epoch();
                        state.totalFeesCollected += HM25_MAINTENANCE_FEE;
                    }
                    
                    state.projects.set(locals.i, locals.project);
                }
                
                // Auto-complete projects when warranty period is over for delivered projects
                if (locals.project.status == HM25_STATUS_DELIVERED && 
                    qpi.epoch() >= locals.project.warrantyEndEpoch) {
                    
                    // Release funds to provider
                    qpi.transfer(locals.project.providerId, locals.project.remainingAmount);
                    
                    // Update status
                    locals.project.status = HM25_STATUS_COMPLETED;
                    locals.project.remainingAmount = 0;
                    
                    // Save updated project
                    state.projects.set(locals.i, locals.project);
                    
                    // Log event
                    HM25Logger log;
                    log._contractIndex = HM25_CONTRACT_INDEX;
                    log._type = LogProjectCompleted;
                    log.projectId = locals.i;
                    log.providerId = locals.project.providerId;
                    log.amount = locals.project.remainingAmount;
                    LOG_INFO(log);
                }
            }
        }
        
        // Distribute collected fees to shareholders if applicable
        locals.amountToDistribute = div(state.totalFeesCollected - state.totalFeesPaid, uint64(NUMBER_OF_COMPUTORS));
        
        if (locals.amountToDistribute > 0 && state.totalFeesCollected > state.totalFeesPaid) {
            if (qpi.distributeDividends(locals.amountToDistribute)) {
                state.totalFeesPaid += locals.amountToDistribute * NUMBER_OF_COMPUTORS;
            }
        }
    _

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES
        // Procedures
        REGISTER_USER_PROCEDURE(createProject, 1);
        REGISTER_USER_PROCEDURE(fundProject, 2);
        REGISTER_USER_PROCEDURE(deliverProject, 3);
        REGISTER_USER_PROCEDURE(approveProject, 4);
        REGISTER_USER_PROCEDURE(cancelProject, 5);
        REGISTER_USER_PROCEDURE(refundProject, 6);
        REGISTER_USER_PROCEDURE(payMaintenance, 7);
        
        // Functions
        REGISTER_USER_FUNCTION(getProjectInfo, 8);
        REGISTER_USER_FUNCTION(getClientProjects, 9);
        REGISTER_USER_FUNCTION(getProviderProjects, 10);
    _
};
