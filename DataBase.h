//==================================================================================================
//                           MÓDULO DE GERENCIAMENTO DA CONEXÃO FIREBASE
//==================================================================================================
#ifndef _DATABASE_MANAGER_
#define _DATABASE_MANAGER_

//==================================================================================================
// Funções
//==================================================================================================
extern void initDataBaseManager(void);
extern void setupDataBase(void);
extern void sendDataToDatabase(char *packet);
extern void disconnectDataBase(void);

#endif _DATABASE_MANAGER_
//==================================================================================================