#include "utils.h"
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <sstream> // Necesario para parsear comandos

using namespace std;

// --- Estilos de Consola (Funcionalidad Extra) ---
const string C_RESET = "\033[0m";
const string C_RED = "\033[31m";
const string C_GREEN = "\033[32m";
const string C_YELLOW = "\033[33m";
const string C_MAGENTA = "\033[35m";
const string C_CYAN = "\033[36m";
const string C_BOLD = "\033[1m";

// --- Constantes del Protocolo ---
const int MSG_TYPE_PUBLIC = 0;
const int MSG_TYPE_PRIVATE = 1;
const int MSG_TYPE_NOTIFICATION = 2; // Mensajes del servidor al cliente

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

    // bucle mientras no salir
    while (!exitChat)
    {

        // Recibir mensaje del servidor
        recvMSG(serverID, buffer);
        if (buffer.size() == 0)
        {
            // conexión cerrada, salir
            if (!exitChat)
            {
                exitChat = true;
                cout << C_RED << "\nEl servidor ha cerrado la conexión inesperadamente." << C_RESET << endl;
            }
            continue;
        }

        // 1. Desempaquetar tipo de mensaje
        int messageType = unpack<int>(buffer);

        // 2. Desempaquetar nombre de usuario (remitente)
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
                 << C_BOLD << username << C_RESET << ": " << message << endl;
            break;
        case MSG_TYPE_PRIVATE: // Mensaje Privado
            cout << "\n"
                 << C_MAGENTA << "(Mensaje privado) " << C_BOLD << username << C_RESET << C_MAGENTA << ": " << message << C_RESET << endl;
            break;
        case MSG_TYPE_NOTIFICATION: // Notificación del Servidor
            // Si es la notificación de 'exit()', solo salimos del bucle
            if (message == "exit()")
            {
                exitChat = true;
                continue; // No imprimas "Notificación: exit()"
            }
            cout << "\n"
                 << C_YELLOW << "Notificación: " << message << C_RESET << endl;
            break;
        }

        cout << C_GREEN << "> " << C_RESET; // Volver a mostrar el prompt
        fflush(stdout);                     // Asegurar que el prompt se imprima
        buffer.clear();

    } // fin del while
}

int main(int argc, char **argv)
{
    vector<unsigned char> buffer;
    string username;
    string inputLine; // Línea completa leída de cin
    string message;   // Mensaje final a enviar
    bool exitChat = false;

    // Pedir nombre de usuario por terminal
    cout << C_CYAN << "Introduzca nombre de usuario:" << C_RESET << endl;
    getline(cin, username);

    // Iniciar conexión al server en localhost:3000
    auto connection = initClient("127.0.0.1", 3000);
    if (connection.socket == -1)
    {
        cout << C_RED << "Error: No se pudo conectar al servidor." << C_RESET << endl;
        return -1;
    }
    cout << C_GREEN << "Conectado al servidor. Escribe 'exit()' para salir o '/msg <usuario> <mensaje>' para mensaje privado." << C_RESET << endl;

    // Iniciar thread "receiveMessages".
    thread *receiveThread = new thread(receiveMessages, connection.serverId, ref(exitChat));

    // --- TAREA: Enviar nombre de usuario al servidor ---
    int usernameLen = username.length();
    pack<int>(buffer, usernameLen);
    packv<char>(buffer, (char *)username.c_str(), usernameLen);
    sendMSG(connection.serverId, buffer);
    buffer.clear();
    // ----------------------------------------------------

    // Bucle hasta que el usuario escribe "exit()"
    do
    {
        cout << C_GREEN << "> " << C_RESET;
        getline(cin, inputLine);

        if (inputLine == "exit()")
        {
            message = "exit()";
            // Empaquetar como mensaje público
            pack<int>(buffer, MSG_TYPE_PUBLIC);
            int messageLen = message.length();
            pack<int>(buffer, messageLen);
            packv<char>(buffer, (char *)message.c_str(), messageLen);
        }
        // --- Implementación de Mensajes Privados ---
        else if (inputLine.rfind("/msg", 0) == 0)
        {
            stringstream ss(inputLine);
            string command;
            string recipientName;

            ss >> command;       // Leer "/msg"
            ss >> recipientName; // Leer "destinatario"

            // Obtener el resto de la línea como el mensaje
            getline(ss, message);
            // Eliminar el espacio en blanco inicial del mensaje
            if (!message.empty() && message[0] == ' ')
            {
                message = message.substr(1);
            }

            if (recipientName.empty() || message.empty())
            {
                cout << C_RED << "Error: Formato incorrecto. Use: /msg <usuario> <mensaje>" << C_RESET << endl;
                continue; // Saltar el envío
            }

            // Empaquetar como mensaje privado
            pack<int>(buffer, MSG_TYPE_PRIVATE);

            int recipientLen = recipientName.length();
            pack<int>(buffer, recipientLen);
            packv<char>(buffer, (char *)recipientName.c_str(), recipientLen);

            int messageLen = message.length();
            pack<int>(buffer, messageLen);
            packv<char>(buffer, (char *)message.c_str(), messageLen);
        }
        // --- Mensaje Público Normal ---
        else
        {
            message = inputLine;
            // Empaquetar como mensaje público
            pack<int>(buffer, MSG_TYPE_PUBLIC);
            int messageLen = message.length();
            pack<int>(buffer, messageLen);
            packv<char>(buffer, (char *)message.c_str(), messageLen);
        }

        // Enviar el buffer (público, privado o de salida)
        sendMSG(connection.serverId, buffer);
        buffer.clear();

    } while (message != "exit()");

    // Marcar variable "salir" para detener el hilo de recepción de mensajes
    exitChat = true;

    receiveThread->join(); // sincronizar con el thread antes de cerrar la conexión

    // Cerrar conexión con el servidor
    closeConnection(connection.serverId);

    cout << "Desconectado." << endl;

    return 0;
}
