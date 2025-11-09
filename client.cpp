#include "utils.h"
#include <string>
#include <iostream>
#include <thread> // Necesario para std::thread
#include <vector> // Necesario para std::vector
#include <sstream>

using namespace std;

const int MSG_TYPE_PUBLIC = 0;
const int MSG_TYPE_PRIVATE = 1;
const int MSG_TYPE_NOTIFICATION = 2;

/**
 * @brief Función para recibir en paralelo mensajes reenviados por el servidor
 * @param serverID ID del socket del servidor
 * @param exitChat Referencia a la variable bool para controlar la salida
 */
void receiveMessages(int serverID, bool &exitChat)
{
    vector<unsigned char> buffer;
    string username;
    string message;

    while (!exitChat)
    {

        recvMSG(serverID, buffer);

        if (buffer.size() == 0)
        {
            if (!exitChat)
            {
                exitChat = true;
                cout << "El servidor ha cerrado la conexión." << endl;
            }
            continue;
        }

        // 1. Desempaquetar tipo de mensaje
        int messageType = unpack<int>(buffer);

        // 2. Desempaquetar nombre de usuario
        int usernameLen = unpack<int>(buffer);
        username.resize(usernameLen);
        unpackv<char>(buffer, (char *)username.data(), usernameLen);

        // 3. Desempaquetar mensaje
        int messageLen = unpack<int>(buffer);
        message.resize(messageLen);
        unpackv<char>(buffer, (char *)message.data(), messageLen);

        // 4. Mostrar según el tipo
        switch (messageType)
        {
        case MSG_TYPE_PUBLIC: // Mensaje Público
            cout << "\n"
                 << username << " dice: " << message << endl;
            break;
        case MSG_TYPE_PRIVATE: // Mensaje Privado
            cout << "\n"
                 << "(Mensaje privado) " << username << " dice: " << message << endl;
            break;
        case MSG_TYPE_NOTIFICATION: // Notificación del Servidor
            // Si es la notificación de 'exit()', solo salimos del bucle
            if (message == "exit()")
            {
                exitChat = true;
                continue; // No imprimas "Notificación: exit()"
            }
            cout << "\n"
                 << "Notificación: " << message << endl;
            break;
        }

        cout << "> "; // Volver a mostrar el prompt
        fflush(stdout);
        buffer.clear();
    }
}

int main(int argc, char **argv)
{
    vector<unsigned char> buffer;

    string username;
    string inputLine;
    string message;
    bool exitChat = false;

    cout << "Introduzca nombre de usuario:" << endl;
    getline(cin, username);

    auto connection = initClient("127.0.0.1", 3000);
    if (connection.socket == -1)
    {
        cout << "Error: No se pudo conectar al servidor." << endl;
        return -1;
    }

    thread *receiveThread = new thread(receiveMessages, connection.serverId, ref(exitChat));

    int usernameLen = username.length();
    pack<int>(buffer, usernameLen);
    packv<char>(buffer, (char *)username.c_str(), usernameLen);
    sendMSG(connection.serverId, buffer);
    buffer.clear();

    // Bucle hasta que el usuario escribe "exit()"
    do
    {
        getline(cin, inputLine);

        // --- LÓGICA DE MENSAJE PRIVADO ---
        if (inputLine.rfind("/msg", 0) == 0)
        {
            stringstream ss(inputLine);
            string command, recipientName;

            ss >> command;        // Leer "/msg"
            ss >> recipientName;  // Leer "destinatario"
            getline(ss, message); // Leer el resto
            if (!message.empty() && message[0] == ' ')
            {
                message = message.substr(1); // Quitar espacio
            }

            if (recipientName.empty() || message.empty())
            {
                cout << "Error: Formato incorrecto. Use: /msg <usuario> <mensaje>" << endl;
                continue;
            }

            pack<int>(buffer, MSG_TYPE_PRIVATE); // Tipo 1

            int recipientLen = recipientName.length();
            pack<int>(buffer, recipientLen);
            packv<char>(buffer, (char *)recipientName.c_str(), recipientLen);

            int messageLen = message.length();
            pack<int>(buffer, messageLen);
            packv<char>(buffer, (char *)message.c_str(), messageLen);
        }
        // --- MENSAJE PÚBLICO O EXIT ---
        else
        {
            message = inputLine;
            pack<int>(buffer, MSG_TYPE_PUBLIC); // Tipo 0

            int messageLen = message.length();
            pack<int>(buffer, messageLen);
            packv<char>(buffer, (char *)message.c_str(), messageLen);
        }

        sendMSG(connection.serverId, buffer);
        buffer.clear();

    } while (message != "exit()");

    exitChat = true;

    receiveThread->join();

    closeConnection(connection.serverId);
    cout << "Desconectado." << endl;

    return 0;
}