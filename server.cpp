#include "utils.h"
#include <iostream>
#include <string>
#include <thread>
#include <list>
#include <vector>    // Necesario para std::vector
#include <algorithm> // Necesario para std::find
#include <mutex>     // Necesario para proteger la lista de usuarios

using namespace std;

// Mutex para proteger la lista de usuarios compartida
mutex users_mutex;

/**
 * @brief Función para atender la conexión de un cliente en un hilo separado
 * @param clientID ID del socket del cliente
 * @param users Lista compartida de IDs de clientes conectados
 */
void handleConnection(int clientID, list<int> &users)
{
    vector<unsigned char> buffer;
    string username;
    string message;

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
        users.push_back(clientID);
    }

    // Bucle repetir mientras no reciba mensaje "exit()" del cliente.
    do
    {
        // Recibir mensaje de texto del cliente
        // recvMSG(clientID, buffer);

        // si hay datos en el buffer, recibir
        if (buffer.size() > 0)
        {

            // --- TAREA: Reconstruir mensaje ---
            // Para reconstruir un mensaje de texto:
            // - desempaquetar del buffer la longitud del mensaje (int)
            // - redimensionar variable mensaje con esa longitud
            // - desempaquetar el texto del mensaje en los datos de la variable mensaje (char*)
            // ----------------------------------

            // mostrar mensaje recibido
            cout << "Mensaje recibido: " << username << " dice: " << message << endl; //

            // --- TAREA: Reenviar a todos (broadcast) ---
            // crear buffer de envío que contenga datos de nombre de usuario y texto
            // empaquetar tamaño de nombreUsuario
            // empaquetar datos de nombreUsuario
            // empaquetar tamaño de mensaje
            // empaquetar datos de mensaje

            // bucle por cada identificador almacenado en la lista "users"
            lock_guard<mutex> lock(users_mutex); // Proteger la lista durante la iteración
            for (auto &userID : users)
            {
                // enviar nuevo buffer usando "userld", sin reenviar al que lo envió
                if (userID != clientID)
                {
                    // sendMSG(userID, buffer);
                }
            }

            // IMPORTANTE limpiar buffer una vez usado:
            // buffer.clear();
            //  -------------------------------------------
        }
        else
        {
            // si no hay datos, error de conexión (el cliente cerró sin enviar mensaje de "exit()")
            cout << "Error: " << username << " cerró inesperadamente." << endl;
            message = "exit()"; // Forzar salida del bucle
        }
    } while (message != "exit()"); //

    // eliminar al cliente de la lista (protegido por mutex)
    {
        lock_guard<mutex> lock(users_mutex);
        users.erase(find(users.begin(), users.end(), clientID));
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

    list<int> usersList; //

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
        thread *clientThread = new thread(handleConnection, newClientID, ref(usersList));
        clientThread->detach(); // El hilo se ejecutará de forma independiente
    }

    close(serverSocketFD); // cerrar el servidor
    return 0;
}