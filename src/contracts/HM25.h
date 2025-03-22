using namespace QPI;

constexpr uint64 PPG_REGISTRATION_FEE = 5000000ULL;
constexpr uint64 PPG_MIN_GUARANTEE_PERIOD = 7ULL; // 7 epochs minimum for guarantee period
constexpr uint64 PPG_MAX_DESCRIPTION_LENGTH = 256;

struct ProjectPaymentGateway : public ContractBase
{
public:
    enum ProjectStatus {
        PENDING = 0,
        IN_PROGRESS = 1, 
        COMPLETED = 2,
        CANCELED_BY_CLIENT = 3,
        EXPIRED = 4,
        FUNDS_RELEASED = 5
    };

    struct Project {
        uint64 projectId;
        id clientWallet;
        id providerWallet;
        uint64 totalAmount;
        uint64 startEpoch;
        uint64 deadlineEpoch;
        uint64 guaranteePeriod; // Number of epochs for guarantee period
        uint32 descriptionLength;
        Array<char, PPG_MAX_DESCRIPTION_LENGTH> description;
        ProjectStatus status;
        bit clientApproved;
        bit vaultCreated;
        uint64 vaultId;
    };

    struct Logger {
        uint32 _contractIndex;
        uint32 _type; // 1: Project created, 2: Project canceled, 3: Project completed, 4: Funds released, 5: Error
        uint64 projectId;
        id caller;
        uint64 amount;
        ProjectStatus status;
        sint8 _terminator;
    };

    // Create a new project
    struct createProject_input {
        id providerWallet;
        uint64 totalAmount;
        uint64 deadlineEpoch; // Absolute epoch number for deadline
        uint64 guaranteePeriod; // Number of epochs for guarantee period
        uint32 descriptionLength;
        Array<char, PPG_MAX_DESCRIPTION_LENGTH> description;
    };
    struct createProject_output {
        uint64 projectId;
    };
    struct createProject_locals {
        Logger logger;
        sint64 slotIndex;
        Project newProject;
    };

    // Start project (provider confirms)
    struct startProject_input {
        uint64 projectId;
    };
    struct startProject_output {};
    struct startProject_locals {
        Project project;
        Logger logger;
        bool isProvider;
    };

    // Client approves project completion
    struct approveCompletion_input {
        uint64 projectId;
    };
    struct approveCompletion_output {};
    struct approveCompletion_locals {
        Project project;
        Logger logger;
        bool isClient;
    };

    // Client cancels project
    struct cancelProject_input {
        uint64 projectId;
    };
    struct cancelProject_output {};
    struct cancelProject_locals {
        Project project;
        Logger logger;
        bool isClient;
        uint64 elapsedEpochs;
        uint64 totalProjectEpochs;
        uint64 providerAmount;
        uint64 clientRefund;
    };

    // Release funds after guarantee period
    struct releaseFunds_input {
        uint64 projectId;
    };
    struct releaseFunds_output {};
    struct releaseFunds_locals {
        Project project;
        Logger logger;
        bool guaranteePeriodEnded;
    };

    // Check project status
    struct getProjectStatus_input {
        uint64 projectId;
    };
    struct getProjectStatus_output {
        ProjectStatus status;
        uint64 deadlineEpoch;
        uint64 guaranteeEndEpoch;
        bit clientApproved;
    };
    struct getProjectStatus_locals {
        Project project;
    };

protected:
    Array<Project, 65536> projects; // Max 2^16 projects
    uint64 nextProjectId;
    uint64 totalProjects;
    uint64 totalCompleted;
    uint64 totalCanceled;
    uint64 totalExpired;
};
