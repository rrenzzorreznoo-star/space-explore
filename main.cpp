#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
// Maps URL paths to files on disk
std::map<std::string, std::string> url_mappings = {
    {"/",          "templates/index.html"},
    {"/about",     "templates/about.html"},
    {"/style.css", "templates/style.css"},
    {"/script.js", "templates/script.js"}
};
// Returns the correct Content-Type header based on file extension
std::string get_content_type(const std::string& path) {
    if (path.find(".css") != std::string::npos) return "text/css";
    if (path.find(".js")  != std::string::npos) return "application/javascript";
    return "text/html";
}
// Reads a file from disk and returns its content as a string
std::string read_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();   // dump entire file into the stream
    return ss.str();
}
// Strips the base path prefix from the URL
// e.g. "/cpp/about" -> "/about"  so the map lookup stays simple
std::string strip_base_path(const std::string& path, const std::string& base) {
    if (!base.empty() && path.find(base) == 0) {
        std::string stripped = path.substr(base.size());
        if (stripped.empty()) stripped = "/";
        return stripped;
    }
    return path;
}
// Builds and sends the HTTP response for one request
void handle_get_request(int client_socket, const std::string& request_path) {
    std::string response;
    auto it = url_mappings.find(request_path);
    if (it != url_mappings.end()) {
        // Path found in our map — read the file
        std::string file_content = read_file(it->second);
        if (!file_content.empty()) {
            std::string content_type = get_content_type(request_path);
            response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: " + content_type + "\r\n"
                "Connection: close\r\n"
                "\r\n" +           // blank line = end of headers
                file_content;      // body
        } else {
            std::cerr << "Failed to open: " << it->second << std::endl;
            response =
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/html\r\nConnection: close\r\n\r\n"
                "<h1>500 Internal Server Error</h1>";
        }
    } else {
        // Path not in map
        response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\nConnection: close\r\n\r\n"
            "<h1>404 Not Found</h1>";
    }
    send(client_socket, response.c_str(), response.size(), 0);
    close(client_socket);
}
// Reads the first line of a raw HTTP request and returns method + path
// e.g. "GET /about HTTP/1.1" -> ("GET", "/about")
std::pair<std::string, std::string> parse_request(const std::string& raw_request) {
    std::istringstream stream(raw_request);
    std::string first_line;
    std::getline(stream, first_line);
    std::istringstream line_stream(first_line);
    std::string method, path;
    line_stream >> method >> path;
    return std::make_pair(method, path);
}
void run_server() {
    // Read PORT and BASE_PATH from environment variables
    const char* port_env  = std::getenv("PORT");
    const char* base_env  = std::getenv("BASE_PATH");
    int port              = port_env ? std::atoi(port_env) : 8008;
    std::string base      = base_env ? std::string(base_env) : "";
    // 1. Create the socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create server socket" << std::endl;
        return;
    }
    // Allow reuse of the port immediately after restart
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 2. Set address: listen on all interfaces, chosen port
    sockaddr_in server_address{};
    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;   // 0.0.0.0
    server_address.sin_port        = htons(port);
    // 3. Bind the socket to the address
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Failed to bind on port " << port << std::endl;
        close(server_socket);
        return;
    }
    // 4. Start listening (up to 10 queued connections)
    if (listen(server_socket, 10) == -1) {
        std::cerr << "Failed to listen" << std::endl;
        close(server_socket);
        return;
    }
    std::cout << "Server listening on port " << port
              << " (base path: " << (base.empty() ? "/" : base) << ")"
              << std::endl;
    // 5. Accept loop — runs forever
    while (true) {
        sockaddr_in client_address{};
        socklen_t   client_address_size = sizeof(client_address);
        // Block until a browser connects
        int client_socket = accept(
            server_socket,
            (struct sockaddr*)&client_address,
            &client_address_size
        );
        if (client_socket == -1) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }
        std::cout << "Client: " << inet_ntoa(client_address.sin_addr) << std::endl;
        // Read the HTTP request into a buffer
        char buffer[4096] = {};
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            close(client_socket);
            continue;
        }
        std::string request_data(buffer, bytes_received);
        auto [method, raw_path] = parse_request(request_data);
        // Strip /cpp prefix, then look up in url_mappings
        std::string path = strip_base_path(raw_path, base);
        std::cout << "Request: " << method << " " << raw_path
                  << " -> lookup: " << path << std::endl;
        handle_get_request(client_socket, path);
    }
    close(server_socket);
}
int main() {
    
    
    run_server();
    
    
    
    return 0;
}
