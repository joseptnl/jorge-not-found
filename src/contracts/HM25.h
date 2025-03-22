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
        id clientWallet;      // Cartera del cliente (origen de los fondos)
        id providerWallet;    // Cartera del proveedor (destino final)
        uint64 totalAmount;
        uint64 startEpoch;
        uint64 deadlineEpoch;
        uint64 guaranteePeriod;
        uint32 descriptionLength;
        ProjectStatus status;
        bit clientApproved;
        uint64 vaultId;       // ID del vault donde se guardan los fondos
    };

    struct Logger {
        uint32 _contractIndex;
        uint32 _type; // 1: Project created, 2: Project canceled, 3: Project completed, 4: Funds released, 5: Error
        uint64 projectId;
        id clientWallet;      // Cartera del cliente (origen de los fondos)
        id providerWallet; 
        id caller;
        uint64 amount;
        ProjectStatus status;
        sint8 _terminator;
        uint64 vaultId;    
    };

    // Create a new project
    struct createProject_input {
        id clientWallet;      // Cartera del cliente (origen de los fondos)
        id providerWallet; 
        uint64 totalAmount;
        uint64 deadlineEpoch; // Absolute epoch number for deadline
        uint64 guaranteePeriod; // Number of epochs for guarantee period
        uint32 descriptionLength;
        
    };
    struct createProject_output {
        uint64 projectId;
    };
    struct createProject_locals {
        Logger logger;
        sint64 slotIndex;
        Project newProject;
        Array<id, MSVAULT_MAX_OWNERS> owners;
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
        MSVAULT_releaseTo_input msReleaseProvider;
        MSVAULT_releaseTo_input msReleaseClient;
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
        MSVAULT_releaseTo_input msRelease;
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

    // Crear un nuevo proyecto (llamado por el cliente)
    EXPORTED_FUNCTION_WITH_LOCALS(createProject)
    {
        // Validar que se haya enviado el monto completo del proyecto + tarifa
        if (qpi.invocationReward() < PPG_REGISTRATION_FEE + input.totalAmount) {
            return;
        }
        
        // Validar parámetros de entrada
        if (input.providerWallet == NULL_ID || 
            input.providerWallet == qpi.caller() || 
            input.deadlineEpoch <= qpi.epoch() ||
            input.guaranteePeriod < PPG_MIN_GUARANTEE_PERIOD) {
            return;
        }
        
        // Buscar slot libre para el nuevo proyecto
        for (locals.slotIndex = 0; locals.slotIndex < 65536; locals.slotIndex++) {
            locals.newProject = state.projects.get(locals.slotIndex);
            if (locals.newProject.projectId == 0) {
                break;
            }
        }
        
        if (locals.slotIndex == 65536) {
            return; // No hay slots disponibles
        }
        
        // Configurar el nuevo proyecto
        locals.newProject.projectId = state.nextProjectId + 1;
        locals.newProject.clientWallet = qpi.caller();      // Cartera del cliente
        locals.newProject.providerWallet = input.providerWallet; // Cartera del proveedor
        locals.newProject.totalAmount = input.totalAmount;
        locals.newProject.startEpoch = qpi.epoch();
        locals.newProject.deadlineEpoch = input.deadlineEpoch;
        locals.newProject.guaranteePeriod = input.guaranteePeriod;
        locals.newProject.descriptionLength = input.descriptionLength;
        locals.newProject.description = input.description;
        locals.newProject.status = ProjectStatus::PENDING;
        locals.newProject.clientApproved = false;
        
        // Crear un vault multifirma para los fondos del proyecto
        // Preparar parámetros para llamada al contrato MSVAULT
        locals.owners.set(0, qpi.caller());          // Cliente como primer propietario
        locals.owners.set(1, input.providerWallet);  // Proveedor como segundo propietario
        
        // Llamar al contrato MSVAULT para crear el vault
        MSVAULT_registerVault_input msInput;
        msInput.vaultName = locals.newProject.projectId;
        msInput.owners = locals.owners;
        msInput.requiredApprovals = 2;  // Requiere aprobación de ambos para liberar fondos
        
        // Llamar al contrato MSVAULT y obtener el ID del vault
        uint64 vaultId = qpi.callContract(11, msInput, input.totalAmount); // 11 es el índice de MSVAULT
        
        locals.newProject.vaultId = vaultId;
        
        // Guardar el proyecto en el estado
        state.projects.set(locals.slotIndex, locals.newProject);
        state.nextProjectId++;
        state.totalProjects++;
        
        // Registrar evento
        locals.logger._contractIndex = 13; // Asumiendo que este es el contrato 13
        locals.logger._type = 1; // Proyecto creado
        locals.logger.projectId = locals.newProject.projectId;
        locals.logger.caller = qpi.caller();
        locals.logger.amount = input.totalAmount;
        locals.logger.status = ProjectStatus::PENDING;
        qpi.log(locals.logger);
        
        output.projectId = locals.newProject.projectId;
    }

    // Proveedor inicia el proyecto
    EXPORTED_FUNCTION_WITH_LOCALS(startProject)
    {
        // Buscar proyecto
        for (uint64 i = 0; i < 65536; i++) {
            locals.project = state.projects.get(i);
            if (locals.project.projectId == input.projectId) {
                locals.isProvider = (locals.project.providerWallet == qpi.caller());
                
                if (!locals.isProvider) {
                    return; // Solo el proveedor puede iniciar el proyecto
                }
                
                if (locals.project.status != ProjectStatus::PENDING) {
                    return; // El proyecto debe estar en estado PENDING
                }
                
                // Actualizar estado
                locals.project.status = ProjectStatus::IN_PROGRESS;
                state.projects.set(i, locals.project);
                
                // Registrar evento
                locals.logger._contractIndex = 13;
                locals.logger._type = 1; // Proyecto iniciado
                locals.logger.projectId = input.projectId;
                locals.logger.caller = qpi.caller();
                locals.logger.status = ProjectStatus::IN_PROGRESS;
                qpi.log(locals.logger);
                
                return;
            }
        }
    }

    // Cliente aprueba la finalización del proyecto
    EXPORTED_FUNCTION_WITH_LOCALS(approveCompletion)
    {
        // Buscar proyecto
        for (uint64 i = 0; i < 65536; i++) {
            locals.project = state.projects.get(i);
            if (locals.project.projectId == input.projectId) {
                // Verificar que quien llama es el cliente
                if (locals.project.clientWallet != qpi.caller()) {
                    return; // Solo el cliente puede aprobar la finalización
                }
                
                if (locals.project.status != ProjectStatus::IN_PROGRESS) {
                    return; // El proyecto debe estar en progreso
                }
                
                // Marcar el proyecto como completado y aprobado por el cliente
                locals.project.status = ProjectStatus::COMPLETED;
                locals.project.clientApproved = true;
                
                // El período de garantía comienza ahora, los fondos se liberarán después
                // del período de garantía automáticamente en el END_EPOCH
                
                state.projects.set(i, locals.project);
                
                // Registrar evento
                locals.logger._contractIndex = 13;
                locals.logger._type = 3; // Proyecto completado
                locals.logger.projectId = input.projectId;
                locals.logger.caller = qpi.caller();
                locals.logger.status = ProjectStatus::COMPLETED;
                qpi.log(locals.logger);
                
                return;
            }
        }
    }

    // Cliente cancela el proyecto
    EXPORTED_FUNCTION_WITH_LOCALS(cancelProject)
    {
        // Buscar proyecto
        for (uint64 i = 0; i < 65536; i++) {
            locals.project = state.projects.get(i);
            if (locals.project.projectId == input.projectId) {
                // Verificar que quien llama es el cliente
                if (locals.project.clientWallet != qpi.caller()) {
                    return; // Solo el cliente puede cancelar
                }
                
                if (locals.project.status != ProjectStatus::IN_PROGRESS) {
                    return; // Solo se pueden cancelar proyectos en progreso
                }
                
                // Calcular distribución de fondos basada en tiempo transcurrido
                locals.elapsedEpochs = qpi.epoch() - locals.project.startEpoch;
                locals.totalProjectEpochs = locals.project.deadlineEpoch - locals.project.startEpoch;
                
                // Calcular cuánto recibe el proveedor (proporcional al tiempo transcurrido)
                if (locals.elapsedEpochs >= locals.totalProjectEpochs) {
                    // Si ya pasó todo el tiempo, el proveedor recibe todo
                    locals.providerAmount = locals.project.totalAmount;
                    locals.clientRefund = 0;
                } else {
                    // Proporción del tiempo transcurrido
                    locals.providerAmount = (locals.project.totalAmount * locals.elapsedEpochs) / locals.totalProjectEpochs;
                    locals.clientRefund = locals.project.totalAmount - locals.providerAmount;
                }
                
                // Preparar transacciones para distribuir los fondos
                if (locals.providerAmount > 0) {
                    // Enviar fondos al proveedor
                    locals.msReleaseProvider.vaultId = locals.project.vaultId;
                    locals.msReleaseProvider.amount = locals.providerAmount;
                    locals.msReleaseProvider.destination = locals.project.providerWallet;
                    
                    // Llamar a MSVAULT para liberar fondos
                    qpi.callContract(11, locals.msReleaseProvider, 0);
                }
                
                if (locals.clientRefund > 0) {
                    // Devolver fondos restantes al cliente
                    locals.msReleaseClient.vaultId = locals.project.vaultId;
                    locals.msReleaseClient.amount = locals.clientRefund;
                    locals.msReleaseClient.destination = locals.project.clientWallet;
                    
                    // Llamar a MSVAULT para liberar fondos
                    qpi.callContract(11, locals.msReleaseClient, 0);
                }
                
                // Actualizar estado del proyecto
                locals.project.status = ProjectStatus::CANCELED_BY_CLIENT;
                state.projects.set(i, locals.project);
                state.totalCanceled++;
                
                // Registrar evento
                locals.logger._contractIndex = 13;
                locals.logger._type = 2; // Proyecto cancelado
                locals.logger.projectId = input.projectId;
                locals.logger.caller = qpi.caller();
                locals.logger.status = ProjectStatus::CANCELED_BY_CLIENT;
                qpi.log(locals.logger);
                
                return;
            }
        }
    }

    // Función para liberar fondos después del período de garantía
    EXPORTED_FUNCTION_WITH_LOCALS(releaseFunds)
    {
        // Buscar proyecto
        for (uint64 i = 0; i < 65536; i++) {
            locals.project = state.projects.get(i);
            if (locals.project.projectId == input.projectId) {
                // Verificar estado y período de garantía
                if (locals.project.status != ProjectStatus::COMPLETED || !locals.project.clientApproved) {
                    return; // El proyecto debe estar completado y aprobado
                }
                
                locals.guaranteePeriodEnded = qpi.epoch() >= (locals.project.deadlineEpoch + locals.project.guaranteePeriod);
                
                if (!locals.guaranteePeriodEnded) {
                    return; // El período de garantía no ha terminado
                }
                
                // Liberar todos los fondos al proveedor
                locals.msRelease.vaultId = locals.project.vaultId;
                locals.msRelease.amount = locals.project.totalAmount;
                locals.msRelease.destination = locals.project.providerWallet;
                
                // Llamar a MSVAULT para liberar fondos
                qpi.callContract(11, locals.msRelease, 0);
                
                // Actualizar estado del proyecto
                locals.project.status = ProjectStatus::FUNDS_RELEASED;
                state.projects.set(i, locals.project);
                state.totalCompleted++;
                
                // Registrar evento
                locals.logger._contractIndex = 13;
                locals.logger._type = 4; // Fondos liberados
                locals.logger.projectId = input.projectId;
                locals.logger.caller = qpi.caller();
                locals.logger.amount = locals.project.totalAmount;
                locals.logger.status = ProjectStatus::FUNDS_RELEASED;
                qpi.log(locals.logger);
                
                return;
            }
        }
    }
    
    // Consultar estado del proyecto
    EXPORTED_FUNCTION_WITH_LOCALS(getProjectStatus)
    {
        // Buscar proyecto
        for (uint64 i = 0; i < 65536; i++) {
            locals.project = state.projects.get(i);
            if (locals.project.projectId == input.projectId) {
                output.status = locals.project.status;
                output.deadlineEpoch = locals.project.deadlineEpoch;
                output.guaranteeEndEpoch = locals.project.deadlineEpoch + locals.project.guaranteePeriod;
                output.clientApproved = locals.project.clientApproved;
                return;
            }
        }
    }

    // Mantener vaults activos y verificar proyectos expirados
    END_EPOCH_WITH_LOCALS
    {
        Project project;
        uint64 vaultFee = MSVAULT_HOLDING_FEE;
        MSVAULT_releaseTo_input msRelease;
        
        // Revisar todos los proyectos para expiración y pagar fees de vault
        for (uint64 i = 0; i < 65536; i++) {
            project = state.projects.get(i);
            
            if (project.projectId == 0) {
                continue; // Slot vacío
            }
            
            // Mantener el vault activo pagando fees
            if (project.status == ProjectStatus::IN_PROGRESS || 
                project.status == ProjectStatus::COMPLETED) {
                
                // Pagar fee al vault para mantenerlo activo
                MSVAULT_deposit_input depositInput;
                depositInput.vaultId = project.vaultId;
                qpi.callContract(11, depositInput, vaultFee);
            }
            
            // Verificar si el proyecto expiró (pasó la fecha límite sin completarse)
            if (project.status == ProjectStatus::IN_PROGRESS && qpi.epoch() > project.deadlineEpoch) {
                // Proyecto expirado - devolver todos los fondos al cliente
                msRelease.vaultId = project.vaultId;
                msRelease.amount = project.totalAmount;
                msRelease.destination = project.clientWallet;
                
                // Llamar a MSVAULT para liberar fondos
                qpi.callContract(11, msRelease, 0);
                
                project.status = ProjectStatus::EXPIRED;
                state.projects.set(i, project);
                state.totalExpired++;
                
                // Registrar evento
                Logger logger;
                logger._contractIndex = 13;
                logger._type = 5; // Error: Proyecto expirado
                logger.projectId = project.projectId;
                logger.status = ProjectStatus::EXPIRED;
                qpi.log(logger);
            }
            
            // Verificar si terminó el período de garantía para proyectos completados
            else if (project.status == ProjectStatus::COMPLETED && 
                    project.clientApproved && 
                    qpi.epoch() >= (project.deadlineEpoch + project.guaranteePeriod)) {
                
                // Período de garantía terminado - liberar todos los fondos al proveedor
                msRelease.vaultId = project.vaultId;
                msRelease.amount = project.totalAmount;
                msRelease.destination = project.providerWallet;
                
                // Llamar a MSVAULT para liberar fondos
                qpi.callContract(11, msRelease, 0);
                
                project.status = ProjectStatus::FUNDS_RELEASED;
                state.projects.set(i, project);
                state.totalCompleted++;
                
                // Registrar evento
                Logger logger;
                logger._contractIndex = 13;
                logger._type = 4; // Fondos liberados
                logger.projectId = project.projectId;
                logger.amount = project.totalAmount;
                logger.status = ProjectStatus::FUNDS_RELEASED;
                qpi.log(logger);
            }
        }    
    }
    _

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES
    REGISTER_USER_PROCEDURE(createProject, 1);
    REGISTER_USER_PROCEDURE(startProject, 2);
    REGISTER_USER_PROCEDURE(approveCompletion, 3);
    REGISTER_USER_PROCEDURE(cancelProject, 4);
    REGISTER_USER_PROCEDURE(releaseFunds, 5);
    REGISTER_USER_FUNCTION(getProjectStatus, 6);
    
    -
};
