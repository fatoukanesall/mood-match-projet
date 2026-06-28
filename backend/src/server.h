#ifndef SERVER_H
#define SERVER_H

int demarrer_serveur(unsigned short port, const char *db_path);
void arreter_serveur(void);

#endif
