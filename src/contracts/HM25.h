#include qpi.h

using namespace QPI;
struct ConditionalPayment : public QPI::ContractBase
{
    struct Payment
    {
        id recipient;
        sint64 amount;
        Array description; // Ensure the array is parameterized with type and size
        bool isReleased;
    };

    struct Milestone
    {
        Array payments; // Specify type and size
        bool isValidated;
    };

    struct Project
    {
        Array milestones; // Specify type and size
        bool isCompleted;
        sint64 breachGuarantee;
        id client;
        id contractor;
    };

    struct Success_output
    {
        bool okay;
    };

private:
    Array projects; // Specify type and size

public:
    struct CreateProject_input
    {
        sint64 breachGuarantee;
        id client;
        id contractor;
    };

    PUBLIC_PROCEDURE(CreateProject)
        Project newProject;
        newProject.isCompleted = false;
        newProject.breachGuarantee = input.breachGuarantee;
        newProject.client = input.client;
        newProject.contractor = input.contractor;
        projects.push(newProject);
        output.okay = true;
    _

    struct AddMilestone_input
    {
        uint16 projectIndex;
    };

    PUBLIC_PROCEDURE(AddMilestone)
        if (input.projectIndex >= projects.size())
        {
            output.okay = false;
            return;
        }
        Milestone newMilestone;
        newMilestone.isValidated = false;
        projects[input.projectIndex].milestones.push(newMilestone);
        output.okay = true;
    _

    struct AddPayment_input
    {
        uint16 projectIndex;
        uint16 milestoneIndex;
        id recipient;
        sint64 amount;
        Array description; // Ensure the array is parameterized with type and size
    };

    PUBLIC_PROCEDURE(AddPayment)
        if (input.projectIndex >= projects.size() || input.milestoneIndex >= projects[input.projectIndex].milestones.size())
        {
            output.okay = false;
            return;
        }
        Payment newPayment;
        newPayment.recipient = input.recipient;
        newPayment.amount = input.amount;
        newPayment.description = input.description;
        newPayment.isReleased = false;
        projects[input.projectIndex].milestones[input.milestoneIndex].payments.push(newPayment);
        output.okay = true;
    _

    struct ValidateMilestone_input
    {
        uint16 projectIndex;
        uint16 milestoneIndex;
    };

    PUBLIC_PROCEDURE(ValidateMilestone)
        if (input.projectIndex >= projects.size() || input.milestoneIndex >= projects[input.projectIndex].milestones.size())
        {
            output.okay = false;
            return;
        }
        projects[input.projectIndex].milestones[input.milestoneIndex].isValidated = true;
        output.okay = true;
    _

    struct ReleasePayment_input
    {
        uint16 projectIndex;
        uint16 milestoneIndex;
        uint16 paymentIndex;
    };

    PUBLIC_PROCEDURE(ReleasePayment)
        if (input.projectIndex >= projects.size() || input.milestoneIndex >= projects[input.projectIndex].milestones.size() || input.paymentIndex >= projects[input.projectIndex].milestones[input.milestoneIndex].payments.size())
        {
            output.okay = false;
            return;
        }
        Payment& payment = projects[input.projectIndex].milestones[input.milestoneIndex].payments[input.paymentIndex];
        if (projects[input.projectIndex].milestones[input.milestoneIndex].isValidated && !payment.isReleased)
        {
            payment.isReleased = true;
            qpi.transfer(payment.recipient, payment.amount);
            output.okay = true;
        }
        else
        {
            output.okay = false;
        }
    _

    struct BreachGuarantee_input
    {
        uint16 projectIndex;
        bool clientBreach;
    };

    PUBLIC_PROCEDURE(BreachGuarantee)
        if (input.projectIndex >= projects.size())
        {
            output.okay = false;
            return;
        }
        Project& project = projects[input.projectIndex];
        if (input.clientBreach)
        {
            qpi.transfer(project.contractor, project.breachGuarantee);
        }
        else
        {
            qpi.transfer(project.client, project.breachGuarantee);
        }
        project.isCompleted = true;
        output.okay = true;
    _

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES
        REGISTER_USER_PROCEDURE(CreateProject, 1);
        REGISTER_USER_PROCEDURE(AddMilestone, 2);
        REGISTER_USER_PROCEDURE(AddPayment, 3);
        REGISTER_USER_PROCEDURE(ValidateMilestone, 4);
        REGISTER_USER_PROCEDURE(ReleasePayment, 5);
        REGISTER_USER_PROCEDURE(BreachGuarantee, 6);
    _

    INITIALIZE
        // Initialization code if needed
    _
};
