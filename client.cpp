#include "utils.h"
#include <string>
#include <iostream>
#include <thread> // Necesario para std::thread
#include <vector> // Necesario para std::vector

using namespace std;

/**
 * @brief Función para recibir en paralelo mensajes reenviados por el servidor
 * @param serverID ID del socket del servidor
 * @param exitChat Referencia a la variable bool para controlar la salida
 */
void receiveMessages(int serverID, bool &exitChat)
{
    vector<unsigned char> buffer; // Buffer de datos que se recibirán
    string username;              // Variable para almacenar el nombre del usuario
    string message;               // Variable para almacenar el mensaje

    // bucle mientras no salir
    while (!exitChat)
    {

        // Recibir mensaje del servidor
        //  !!AQUÍ DEBES LLAMAR A recvMSG(serverID, buffer);!!

        // si hay datos en el buffer.
        if (buffer.size() > 0)
        {
            // desempaquetar del buffer la longitud del nombreUsuario
            // - redimensionar variable nombreUsuario con esa longitud
            // - desempaquetar el texto del nombre en los datos de la variable nombreUsuario

            // desempaquetar del buffer la longitud del mensaje
            // - redimensionar variable mensaje con esa longitud
            // - desempaquetar el texto del mensaje en los datos de la variable mensaje

            // mostrar mensaje recibido
            cout << "Mensaje recibido: " << username << " dice: " << message << endl;

            // !!AQUÍ DEBES LIMPIAR EL BUFFER: buffer.clear();!!
        }
        else
        {
            // conexión cerrada, salir
            if (!exitChat)
            {
                exitChat = true;
                cout << "El servidor ha cerrado la conexión." << endl;
            }
        }
    }
}

int main(int argc, char **argv)
{
    vector<unsigned char> buffer;

    string username;
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
        getline(cin, message);

        // --- TAREA: Enviar mensaje al servidor ---
        int messageLen = message.length();
        pack<int>(buffer, messageLen);
        packv<char>(buffer, (char *)message.c_str(), messageLen);
        sendMSG(connection.serverId, buffer);
        buffer.clear();
        // ----------------------------------------------------

    } while (message != "exit()");

    exitChat = true;

    receiveThread->join();

    closeConnection(connection.serverId);

    return 0;
}