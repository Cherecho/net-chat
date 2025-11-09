#include "utils.h"
#include <string>
#include <iostream>
#include <thread> // Necesario para std::thread
#include <vector> // Necesario para std::vector
#include <sstream>

using namespace std;

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

        // Recibir mensaje del servidor
        recvMSG(serverID, buffer);

        // si hay datos en el buffer.
        if (buffer.size() > 0)
        {
            // desempaquetar del buffer la longitud del nombreUsuario
            int usernameLen = unpack<int>(buffer);
            username.resize(usernameLen);
            unpackv<char>(buffer, (char *)username.data(), usernameLen);

            // desempaquetar del buffer la longitud del mensaje
            int messageLen = unpack<int>(buffer);
            message.resize(messageLen);
            unpackv<char>(buffer, (char *)message.data(), messageLen);

            // mostrar mensaje recibido
            cout << "Mensaje recibido: " << username << " dice: " << message << endl;

            buffer.clear();
        }
        else
        {
            // El servidor cerró la conexión
            if (!exitChat)
            {
                exitChat = true;
                // No mostramos error si la salida es limpia
            }
        }
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