#include "utils.h"
#include <iostream>
#include <string>
#include <thread>
#include <list>
#include <vector>
#include <algorithm>
#include <mutex>
#include <map>     // Necesario para los mensajes privados
#include <sstream> // Necesario para los mensajes privados

using namespace std;

// --- Estilos de Consola (Funcionalidad Extra) ---
const string C_RESET = "\033[0m";
const string C_RED = "\033[31m";
const string C_GREEN = "\033[32m";
const string C_YELLOW = "\033[33m";
const string C_MAGENTA = "\033[35m";
const string C_CYAN = "\033[36m";

// --- Constantes del Protocolo ---
const int MSG_TYPE_PUBLIC = 0;
const int MSG_TYPE_PRIVATE = 1;
const int MSG_TYPE_NOTIFICATION = 2; // Mensajes del servidor al cliente

// Mutex para proteger el mapa de usuarios
mutex users_mutex;

/**
 * @brief Función para atender la conexión de un cliente en un hilo separado
 * @param clientID ID del socket del cliente
 * @param usersMap Mapa compartido de (nombre, ID) de clientes conectados
 */
void handleConnection(int clientID, map<string, int> &usersMap)
{
    vector<unsigned char> buffer;
    string username;
    string message;
    bool keepRunning = true;

    // --- TAREA: Recibir nombre de usuario ---
    recvMSG(clientID, buffer);

    if (buffer.size() > 0)
    {
        int usernameLen = unpack<int>(buffer);
        username.resize(usernameLen);
        unpackv<char>(buffer, (char *)username.data(), usernameLen);
        buffer.clear();
    }
    else
    {
        cout << C_RED << "Error: Cliente " << clientID << " se conectó sin enviar nombre." << C_RESET << endl;
        closeConnection(clientID);
        return;
    }

    // mostrar mensaje de conexión y añadir al mapa
    cout << C_GREEN << "Usuario Conectado: " << username << " (ID: " << clientID << ")" << C_RESET << endl;
    {
        lock_guard<mutex> lock(users_mutex);
        usersMap[username] = clientID;
    }

    // Bucle principal del hilo
    do
    {
        // Recibir mensaje de texto del cliente
        recvMSG(clientID, buffer);
        if (buffer.size() == 0)
        {
            cout << C_YELLOW << "Error: " << username << " cerró inesperadamente." << C_RESET << endl;
            keepRunning = false; // Forzar salida del bucle
            continue;            // Saltar al final del bucle para la limpieza
        }

        // 1. Desempaquetar el tipo de mensaje
        int messageType = unpack<int>(buffer);

        switch (messageType)
        {
        // --- Caso 0: Mensaje Público (Broadcast) ---
        case MSG_TYPE_PUBLIC:
        {
            // Desempaquetar el mensaje
            int messageLen = unpack<int>(buffer);
            message.resize(messageLen);
            unpackv<char>(buffer, (char *)message.data(), messageLen);

            cout << "Mensaje recibido (Público): " << username << ": " << message << endl;

            // Comprobar si es un mensaje de salida
            if (message == "exit()")
            {
                keepRunning = false; // Salir del bucle do-while
                continue;            // Saltar al final del bucle para la limpieza
            }

            // Re-empaquetar para broadcast
            buffer.clear();
            pack<int>(buffer, MSG_TYPE_PUBLIC); // Tipo 0 = Público

            int usernameLen = username.length();
            pack<int>(buffer, usernameLen);
            packv<char>(buffer, (char *)username.c_str(), usernameLen);

            pack<int>(buffer, messageLen);
            packv<char>(buffer, (char *)message.c_str(), messageLen);

            // Enviar a todos excepto al remitente
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

            cout << C_MAGENTA << "Mensaje recibido (Privado): " << username << " para " << recipientName << C_RESET << endl;

            int recipientID = -1;
            string notificationMessage;

            // Buscar al destinatario en el mapa (protegido)
            {
                lock_guard<mutex> lock(users_mutex);
                if (usersMap.count(recipientName))
                {
                    recipientID = usersMap[recipientName];
                }
            }

            // Si se encuentra, enviar mensaje privado
            if (recipientID != -1)
            {
                // 1. Preparar buffer para el destinatario
                buffer.clear();
                pack<int>(buffer, MSG_TYPE_PRIVATE); // Tipo 1 = Privado

                int usernameLen = username.length();
                pack<int>(buffer, usernameLen);
                packv<char>(buffer, (char *)username.c_str(), usernameLen);

                pack<int>(buffer, messageLen);
                packv<char>(buffer, (char *)message.c_str(), messageLen);

                sendMSG(recipientID, buffer); // Enviar al destinatario

                // 2. Preparar notificación de éxito para el remitente
                notificationMessage = "Mensaje enviado a " + recipientName;
            }
            // Si no se encuentra, notificar al remitente
            else
            {
                notificationMessage = "Error: Usuario '" + recipientName + "' no encontrado.";
            }

            // Enviar notificación de vuelta al remitente
            buffer.clear();
            pack<int>(buffer, MSG_TYPE_NOTIFICATION); // Tipo 2 = Notificación
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

    // --- INICIO SOLUCIÓN "Lost Connection" ---
    // Notificar al cliente que se está cerrando la conexión
    buffer.clear();
    pack<int>(buffer, MSG_TYPE_NOTIFICATION); // Tipo 2 = Notificación
    string serverName = "Servidor";
    int serverNameLen = serverName.length();
    pack<int>(buffer, serverNameLen);
    packv<char>(buffer, (char *)serverName.c_str(), serverNameLen);

    string exitMessage = "exit()";
    int exitMessageLen = exitMessage.length();
    pack<int>(buffer, exitMessageLen);
    packv<char>(buffer, (char *)exitMessage.c_str(), exitMessageLen);

    sendMSG(clientID, buffer); // Enviar confirmación de "exit()"
    // --- FIN SOLUCIÓN ---

    // eliminar al cliente del mapa (protegido)
    {
        lock_guard<mutex> lock(users_mutex);
        usersMap.erase(username);
    }

    cout << C_YELLOW << "Usuario Desconectado: " << username << C_RESET << endl;

    // cerrar conexión con el cliente
    closeConnection(clientID);
}

int main(int argc, char **argv)
{
    // Iniciar el server en el puerto 3000
    auto serverSocketFD = initServer(3000);
    if (serverSocketFD == -1)
    {
        cout << C_RED << "Error al iniciar el servidor." << C_RESET << endl;
        return -1;
    }

    cout << C_GREEN << "Servidor iniciado en el puerto 3000. Esperando conexiones..." << C_RESET << endl;

    // Cambiamos la lista de usuarios por un mapa [nombre -> clientID]
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
        // Se pasa el id y la referencia al mapa compartido
        thread *clientThread = new thread(handleConnection, newClientID, ref(usersMap));
        clientThread->detach();
    }

    close(serverSocketFD); // cerrar el servidor
    return 0;
}