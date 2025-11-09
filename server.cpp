#include "utils.h"
#include <iostream>
#include <string>
#include <thread>
#include <list>
#include <vector>    // Necesario para std::vector
#include <algorithm> // Necesario para std::find
#include <mutex>     // Necesario para proteger la lista de usuarios

using namespace std;

// Definición de tipos de mensajes
const int MSG_TYPE_PUBLIC = 0;
const int MSG_TYPE_PRIVATE = 1;
const int MSG_TYPE_NOTIFICATION = 2;

// Mutex para proteger la lista de usuarios compartida
mutex users_mutex;

/**
 * @brief Función para atender la conexión de un cliente en un hilo separado
 * @param clientID ID del socket del cliente
 * @param users Lista compartida de IDs de clientes conectados
 */
void handleConnection(int clientID, map<string, int> &usersMap)
{
    vector<unsigned char> buffer;
    string username;
    string message;
    bool keepRunning = true;

    // --- TAREA: Recibir nombre de usuario ---
    recvMSG(clientID, buffer);

    // Desempaquetar nombre de usuario:
    if (buffer.size() > 0)
    {
        int usernameLen = unpack<int>(buffer);
        username.resize(usernameLen);
        unpackv<char>(buffer, (char *)username.data(), usernameLen);
        buffer.clear();
    }
    else
    {
        cout << "Error: Cliente " << clientID << " se conectó sin enviar nombre." << endl;
        closeConnection(clientID);
        return;
    }
    // ----------------------------------------

    // mostrar mensaje de conexión
    cout << "Usuario Conectado: " << username << endl; //

    // añadir clientId a la lista "users" compartida (protegido por mutex)
    {
        lock_guard<mutex> lock(users_mutex);
        usersMap[username] = clientID;
    }

    // Bucle repetir mientras no reciba mensaje "exit()" del cliente.
    do
    {
        recvMSG(clientID, buffer);

        if (buffer.size() == 0)
        {
            cout << "Error: " << username << " cerró inesperadamente." << endl;
            keepRunning = false;
            continue;
        }

        // 1. Desempaquetar el tipo de mensaje
        int messageType = unpack<int>(buffer);

        switch (messageType)
        {
            // --- Caso 0: Mensaje Público (Broadcast) ---
            case MSG_TYPE_PUBLIC:
            {
                int messageLen = unpack<int>(buffer);
                message.resize(messageLen);
                unpackv<char>(buffer, (char *)message.data(), messageLen);

                cout << "Mensaje recibido (Público): " << username << " dice: " << message << endl;

                if (message == "exit()")
                {
                    keepRunning = false;
                    continue;
                }

                buffer.clear();
                // AÑADIMOS EL TIPO AL REENVÍO
                pack<int>(buffer, MSG_TYPE_PUBLIC);

                int usernameLen = username.length();
                pack<int>(buffer, usernameLen);
                packv<char>(buffer, (char *)username.c_str(), usernameLen);

                pack<int>(buffer, messageLen);
                packv<char>(buffer, (char *)message.c_str(), messageLen);

                lock_guard<mutex> lock(users_mutex);
                for (auto const &userPair : usersMap)
                {
                    if (userPair.second != clientID)
                    {
                        sendMSG(userPair.second, buffer);
                    }
                }
                buffer.clear();
                break;
            }

            // --- Caso 1: Mensaje Privado ---
            case MSG_TYPE_PRIVATE:
            {
                // Desempaquetar destinatario
                int recipientNameLen = unpack<int>(buffer);
                string recipientName;
                recipientName.resize(recipientNameLen);
                unpackv<char>(buffer, (char *)recipientName.data(), recipientNameLen);

                // Desempaquetar mensaje
                int messageLen = unpack<int>(buffer);
                message.resize(messageLen);
                unpackv<char>(buffer, (char *)message.data(), messageLen);

                cout << "Mensaje recibido (Privado): " << username << " para " << recipientName << endl;

                int recipientID = -1;
                string notificationMessage;

                {
                    lock_guard<mutex> lock(users_mutex);
                    if (usersMap.count(recipientName))
                    {
                        recipientID = usersMap[recipientName];
                    }
                }

                if (recipientID != -1)
                {
                    // 1. Preparar buffer para el destinatario
                    buffer.clear();
                    pack<int>(buffer, MSG_TYPE_PRIVATE); // Tipo 1

                    int usernameLen = username.length();
                    pack<int>(buffer, usernameLen);
                    packv<char>(buffer, (char *)username.c_str(), usernameLen);

                    pack<int>(buffer, messageLen);
                    packv<char>(buffer, (char *)message.c_str(), messageLen);

                    sendMSG(recipientID, buffer); // Enviar al destinatario

                    notificationMessage = "Mensaje enviado a " + recipientName;
                }
                else
                {
                    notificationMessage = "Error: Usuario '" + recipientName + "' no encontrado.";
                }

                // 2. Enviar notificación de vuelta al remitente
                buffer.clear();
                pack<int>(buffer, MSG_TYPE_NOTIFICATION); // Tipo 2
                string serverName = "Servidor";
                int serverNameLen = serverName.length();
                pack<int>(buffer, serverNameLen);
                packv<char>(buffer, (char *)serverName.c_str(), serverNameLen);

                int notificationLen = notificationMessage.length();
                pack<int>(buffer, notificationLen);
                packv<char>(buffer, (char *)notificationMessage.c_str(), notificationLen);

                sendMSG(clientID, buffer);

                buffer.clear();
                break;
            }
        } // fin del switch

    } while (keepRunning);

    buffer.clear();
    pack<int>(buffer, MSG_TYPE_NOTIFICATION); // Tipo 2
    string serverName = "Servidor";
    int serverNameLen = serverName.length();
    pack<int>(buffer, serverNameLen);
    packv<char>(buffer, (char *)serverName.c_str(), serverNameLen);

    string exitMessage = "exit()";
    int exitMessageLen = exitMessage.length();
    pack<int>(buffer, exitMessageLen);
    packv<char>(buffer, (char *)exitMessage.c_str(), exitMessageLen);

    sendMSG(clientID, buffer);

    // eliminar al cliente de la lista (protegido por mutex)
    {
        lock_guard<mutex> lock(users_mutex);
        usersMap.erase(username);
    }

    cout << "Usuario Desconectado: " << username << endl;

    // cerrar conexión con el cliente
    closeConnection(clientID);
}

int main(int argc, char **argv)
{
    // Iniciar el server en el puerto 3000
    auto serverSocketFD = initServer(3000);
    if (serverSocketFD == -1)
    {
        cout << "Error al iniciar el servidor." << endl;
        return -1;
    }

    cout << "Servidor iniciado en el puerto 3000. Esperando conexiones..." << endl;

    map<string, int> usersMap;

    // bucle infinito
    while (1)
    {
        // Esperar conexión de un cliente
        while (!checkClient())
        {
            usleep(1000); // Espera 1ms
        }

        // Una vez conectado, conseguir su identificador
        auto newClientID = getLastClientID();

        // Crear hilo paralelo en el que se ejecuta "handleConnection"
        // Se pasa el id y la referencia a la lista compartida
        thread *clientThread = new thread(handleConnection, newClientID, ref(usersMap));
        clientThread->detach(); // El hilo se ejecutará de forma independiente
    }

    close(serverSocketFD); // cerrar el servidor
    return 0;
}