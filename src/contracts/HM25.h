using namespace QPI;

// Constants
const uint64_t PPG_REGISTRATION_FEE = 5000000ULL;
const uint64_t PPG_MIN_GUARANTEE_PERIOD = 7ULL;
const uint64_t PPG_MAX_DESCRIPTION_LENGTH = 256;

// Type aliases

using id = uint64_t;  // Assuming id is a 64-bit identifier
const id NULL_ID = 0;

enum ProjectStatus {
    PENDING = 0,
    IN_PROGRESS = 1, 
    COMPLETED = 2,
    CANCELED_BY_CLIENT = 3,
    EXPIRED = 4,
    FUNDS_RELEASED = 5
};

struct HM252 
{
};


struct HM25 : public ContractBase
{
public:

    struct Project {
        uint64_t projectId;
        id clientWallet;      // Client's wallet (source of funds)
        id providerWallet;    // Provider's wallet (final destination)
        uint64_t totalAmount;
        uint64_t startEpoch;
        uint64_t deadlineEpoch;
        uint64_t guaranteePeriod;
        uint32_t descriptionLength;
        std::string description;
        ProjectStatus status;
        bool clientApproved;
        uint64_t vaultId;     // ID of the vault where funds are stored
    };

    // Create a new project (called by the client)
    uint64_t createProject(
        id providerWallet,
        uint64_t totalAmount,
        uint64_t deadlineEpoch,
        uint64_t guaranteePeriod,
        uint32_t descriptionLength,
        const std::string& description
    );

    // Provider starts the project
    bool startProject(uint64_t projectId);

    // Client approves project completion
    bool approveCompletion(uint64_t projectId);

    // Client cancels the project
    bool cancelProject(uint64_t projectId);

    // Release funds after guarantee period
    bool releaseFunds(uint64_t projectId);

    // Get project status
    struct ProjectStatusInfo {
        ProjectStatus status;
        uint64_t deadlineEpoch;
        uint64_t guaranteeEndEpoch;
        bool clientApproved;
    };
    
    ProjectStatusInfo getProjectStatus(uint64_t projectId);

    // Process end of epoch (check expired projects, pay vault fees)
    void processEndEpoch();

    // Constructor
    HM25();

private:
    std::vector<Project> projects;  // Project storage
    uint64_t nextProjectId;
    uint64_t totalProjects;
    uint64_t totalCompleted;
    uint64_t totalCanceled;
    uint64_t totalExpired;

    // Utility function to find a project by ID
    Project* findProject(uint64_t projectId);
    
    // Mock functions for contract interaction
    uint64_t callMSVault_registerVault(uint64_t vaultName, const std::vector<id>& owners, uint8_t requiredApprovals, uint64_t amount);
    bool callMSVault_releaseTo(uint64_t vaultId, uint64_t amount, id destination);
    bool callMSVault_deposit(uint64_t vaultId, uint64_t amount);
    
    // System functions
    id getCaller() const;
    uint64_t getCurrentEpoch() const;
    uint64_t getInvocationReward() const;
    
    // Logging function
    void logEvent(uint32_t type, uint64_t projectId, id caller, uint64_t amount, ProjectStatus status, uint64_t vaultId = 0);
};

