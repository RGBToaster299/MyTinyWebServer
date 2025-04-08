// mlws.cpp – My Tiny Web Server (C++ Version)
// By Jamie / RGBToaster

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>
#include <regex>
#include <csignal>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <arpa/inet.h>
#include <netdb.h>  // Added for gethostbyname
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace fs = std::filesystem;

// Global variables for server control
std::atomic<bool> g_running(true);
std::atomic<bool> g_restart(false);
std::mutex g_mutex;
std::condition_variable g_cv;
std::vector<std::string> g_plugins; // Placeholder for plugins
int g_port = 80; // Global port variable

// Logging
enum class LogLevel { INFO, WARN, ERROR, FATAL };

void log(LogLevel level, const std::string& msg) {
    const char* levelStr = "INFO";
    const char* color = "\033[37m";
    switch (level) {
        case LogLevel::INFO: levelStr = "INFO"; color = "\033[37m"; break;
        case LogLevel::WARN: levelStr = "WARN"; color = "\033[33m"; break;
        case LogLevel::ERROR: levelStr = "ERROR"; color = "\033[31m"; break;
        case LogLevel::FATAL: levelStr = "FATAL"; color = "\033[35m"; break;
    }
    time_t now = time(nullptr);
    tm* local = localtime(&now);
    char timebuf[10];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", local);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    std::cout << color << "[ " << timebuf << " = " << levelStr << " ] " << msg << "\033[0m" << std::endl;
    
    if (level == LogLevel::FATAL) {
        g_running = false;
        g_cv.notify_all();
        exit(1);
    }
}

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    if (signum == SIGINT) {
        log(LogLevel::INFO, "Received shutdown signal. Shutting down gracefully...");
        g_running = false;
        g_cv.notify_all();
    }
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string getMimeType(const std::string& filename) {
    if (endsWith(filename, ".html") || endsWith(filename, ".htm")) return "text/html";
    if (endsWith(filename, ".css")) return "text/css";
    if (endsWith(filename, ".js")) return "application/javascript";
    if (endsWith(filename, ".png")) return "image/png";
    if (endsWith(filename, ".jpg") || endsWith(filename, ".jpeg")) return "image/jpeg";
    if (endsWith(filename, ".gif")) return "image/gif";
    if (endsWith(filename, ".svg")) return "image/svg+xml";
    if (endsWith(filename, ".json")) return "application/json";
    if (endsWith(filename, ".ico")) return "image/x-icon";
    if (endsWith(filename, ".pdf")) return "application/pdf";
    if (endsWith(filename, ".mp4")) return "video/mp4";
    if (endsWith(filename, ".mp3")) return "audio/mpeg";
    return "text/plain";
}

// Helper function to get SVG icon for file type
std::string getFileIconSvg(const std::string& filename, std::string& iconColor) {
    std::string iconSvg;
    
    if (endsWith(filename, ".html") || endsWith(filename, ".htm")) {
        iconSvg = "M12,17.56L16.07,16.43L16.62,10.33H9.38L9.2,8.3H16.8L17,6.31H7L7.56,12.32H14.45L14.22,14.9L12,15.5L9.78,14.9L9.64,13.24H7.64L7.93,16.43L12,17.56M4.07,3H19.93L18.5,19.2L12,21L5.5,19.2L4.07,3Z";
        iconColor = "#e44d26";
    } else if (endsWith(filename, ".css")) {
        iconSvg = "M5,3L4.35,6.34H17.94L17.5,8.5H3.92L3.26,11.83H16.85L16.09,16.64L10.61,18.33L5.86,16.64L6.19,14.41H2.87L2.17,19L9.95,22L18.3,19L20.1,3H5Z";
        iconColor = "#264de4";
    } else if (endsWith(filename, ".js")) {
        iconSvg = "M3,3H21V21H3V3M7.73,18.04C8.13,18.89 8.92,19.59 10.27,19.59C11.77,19.59 12.8,18.79 12.8,17.04V11.26H11.1V17C11.1,17.86 10.75,18.08 10.2,18.08C9.62,18.08 9.38,17.68 9.11,17.21L7.73,18.04M13.71,17.86C14.21,18.84 15.22,19.59 16.8,19.59C18.4,19.59 19.6,18.76 19.6,17.23C19.6,15.82 18.79,15.19 17.35,14.57L16.93,14.39C16.2,14.08 15.89,13.87 15.89,13.37C15.89,12.96 16.2,12.64 16.7,12.64C17.18,12.64 17.5,12.85 17.79,13.37L19.1,12.5C18.55,11.54 17.77,11.17 16.7,11.17C15.19,11.17 14.22,12.13 14.22,13.4C14.22,14.78 15.03,15.43 16.25,15.95L16.67,16.13C17.45,16.47 17.91,16.68 17.91,17.26C17.91,17.74 17.46,18.09 16.76,18.09C15.93,18.09 15.45,17.66 15.09,17.06L13.71,17.86Z";
        iconColor = "#f0db4f";
    } else if (endsWith(filename, ".cpp") || endsWith(filename, ".c") || endsWith(filename, ".h")) {
        iconSvg = "M10.5,15.97L10.91,18.41C10.65,18.55 10.23,18.68 9.67,18.8C9.1,18.93 8.43,19 7.66,19C5.45,18.96 3.79,18.3 2.68,17.04C1.56,15.77 1,14.16 1,12.21C1.05,9.9 1.72,8.13 3,6.89C4.32,5.64 5.96,5 7.94,5C8.69,5 9.34,5.07 9.88,5.19C10.42,5.31 10.82,5.44 11.08,5.59L10.5,8.08L9.44,7.74C9.04,7.64 8.58,7.59 8.05,7.59C6.89,7.58 5.93,7.95 5.18,8.69C4.42,9.42 4.03,10.54 4,12.03C4,13.39 4.37,14.45 5.08,15.23C5.79,16 6.79,16.4 8.07,16.41L9.4,16.29C9.83,16.21 10.19,16.1 10.5,15.97M11,11H13V9H15V11H17V13H15V15H13V13H11V11Z";
        iconColor = "#659ad2";
    } else if (endsWith(filename, ".jpg") || endsWith(filename, ".jpeg") || endsWith(filename, ".png") || endsWith(filename, ".gif") || endsWith(filename, ".svg")) {
        iconSvg = "M8.5,13.5L11,16.5L14.5,12L19,18H5M21,19V5C21,3.89 20.1,3 19,3H5A2,2 0 0,0 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19Z";
        iconColor = "#ff9e80";
    } else if (endsWith(filename, ".mp4") || endsWith(filename, ".webm") || endsWith(filename, ".mov")) {
        iconSvg = "M18,4L20,8H17L15,4H13L15,8H12L10,4H8L10,8H7L5,4H4A2,2 0 0,0 2,6V18A2,2 0 0,0 4,20H20A2,2 0 0,0 22,18V4H18Z";
        iconColor = "#ff5252";
    } else if (endsWith(filename, ".mp3") || endsWith(filename, ".wav") || endsWith(filename, ".ogg")) {
        iconSvg = "M14,3.23V5.29C16.89,6.15 19,8.83 19,12C19,15.17 16.89,17.84 14,18.7V20.77C18,19.86 21,16.28 21,12C21,7.72 18,4.14 14,3.23M16.5,12C16.5,10.23 15.5,8.71 14,7.97V16C15.5,15.29 16.5,13.76 16.5,12M3,9V15H7L12,20V4L7,9H3Z";
        iconColor = "#69f0ae";
    } else if (endsWith(filename, ".pdf")) {
        iconSvg = "M19,3A2,2 0 0,1 21,5V19A2,2 0 0,1 19,21H5C3.89,21 3,20.1 3,19V5C3,3.89 3.89,3 5,3H19M10.59,10.08C10.57,10.13 10.3,11.84 8.5,14.77C8.5,14.77 5,14 5.5,11.5C5.83,10.04 6.15,8.95 7.86,9.15C9.82,9.38 10.38,9.96 10.59,10.08M15.5,14.5C14,14.5 11.5,13.26 10,10.5C10.5,9.65 10.62,9.56 10.81,9.43C11.38,9.08 13.69,7.95 15.5,9C17.35,10.05 18.29,10.53 18.38,12.42C18.45,14.16 17,14.5 15.5,14.5M18.5,15C18.5,15 17,18 13,19C9.03,20 5.08,19.5 5.08,19.5C6.45,15.4 7.23,13.5 8.5,12C9.09,13.31 10.5,14.5 12,14.5C14,14.5 14.35,14.25 15.25,13.66C16.23,15.27 18.5,15 18.5,15Z";
        iconColor = "#ff5252";
    } else if (endsWith(filename, ".zip") || endsWith(filename, ".rar") || endsWith(filename, ".tar") || endsWith(filename, ".gz")) {
        iconSvg = "M14,17H12V15H10V13H12V15H14M14,9H12V11H14V13H12V11H10V9H12V7H10V5H12V7H14M19,3H5C3.89,3 3,3.89 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V5C21,3.89 20.1,3 19,3Z";
        iconColor = "#ffd740";
    } else {
        iconSvg = "M14,2H6A2,2 0 0,0 4,4V20A2,2 0 0,0 6,22H18A2,2 0 0,0 20,20V8L14,2M18,20H6V4H13V9H18V20Z";
        iconColor = "#f0f0f0";
    }
    
    return iconSvg;
}

std::string generateExplorerHTML(const std::string& path) {
    std::stringstream html;
    
    // HTML head with styles
    html << "<!DOCTYPE html><html><head><title>MTWS Directory Explorer</title>"
         << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
         << "<style>"
         << ":root {"
         << "  --bg-color: #121212;"
         << "  --card-bg: #1e1e1e;"
         << "  --card-hover: #2a2a2a;"
         << "  --text-color: #f0f0f0;"
         << "  --accent-color: #8a2be2;"
         << "  --accent-hover: #9d44e6;"
         << "  --header-bg: #1a1a1a;"
         << "  --shadow-color: rgba(138, 43, 226, 0.4);"
         << "  --warning-bg: #332700;"
         << "  --warning-border: #665200;"
         << "  --warning-text: #ffcc00;"
         << "  --icon-color: #f0f0f0;"
         << "  --folder-color: #8a85ff;"
         << "  --file-color: #f0f0f0;"
         << "  --file-js-color: #f0db4f;"
         << "  --file-html-color: #e44d26;"
         << "  --file-css-color: #264de4;"
         << "  --file-cpp-color: #659ad2;"
         << "  --file-img-color: #ff9e80;"
         << "  --file-video-color: #ff5252;"
         << "  --file-audio-color: #69f0ae;"
         << "  --file-pdf-color: #ff5252;"
         << "  --file-archive-color: #ffd740;"
         << "}"
         << "* {"
         << "  box-sizing: border-box;"
         << "  margin: 0;"
         << "  padding: 0;"
         << "  transition: all 0.2s ease;"
         << "}"
         << "body {"
         << "  background: var(--bg-color);"
         << "  color: var(--text-color);"
         << "  font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;"
         << "  line-height: 1.6;"
         << "  padding-bottom: 2rem;"
         << "  min-height: 100vh;"
         << "}"
         << "header {"
         << "  background: var(--header-bg);"
         << "  padding: 0.75rem 1rem;"
         << "  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);"
         << "  position: sticky;"
         << "  top: 0;"
         << "  z-index: 100;"
         << "  backdrop-filter: blur(10px);"
         << "  display: flex;"
         << "  align-items: center;"
         << "  justify-content: space-between;"
         << "}"
         << ".header-title {"
         << "  font-size: 1.2rem;"
         << "  font-weight: 600;"
         << "  display: flex;"
         << "  align-items: center;"
         << "  gap: 0.5rem;"
         << "}"
         << ".navigation {"
         << "  display: flex;"
         << "  align-items: center;"
         << "  gap: 0.5rem;"
         << "}"
         << ".nav-button {"
         << "  background: transparent;"
         << "  border: none;"
         << "  color: var(--text-color);"
         << "  width: 36px;"
         << "  height: 36px;"
         << "  border-radius: 4px;"
         << "  display: flex;"
         << "  align-items: center;"
         << "  justify-content: center;"
         << "  cursor: pointer;"
         << "}"
         << ".nav-button:hover {"
         << "  background: rgba(255, 255, 255, 0.1);"
         << "}"
         << ".nav-button svg {"
         << "  width: 20px;"
         << "  height: 20px;"
         << "  fill: var(--icon-color);"
         << "}"
         << ".breadcrumb {"
         << "  display: flex;"
         << "  align-items: center;"
         << "  gap: 0.25rem;"
         << "  margin-left: 1rem;"
         << "  flex-grow: 1;"
         << "  overflow-x: auto;"
         << "  white-space: nowrap;"
         << "  scrollbar-width: thin;"
         << "  padding: 0.25rem 0;"
         << "}"
         << ".breadcrumb::-webkit-scrollbar {"
         << "  height: 4px;"
         << "}"
         << ".breadcrumb::-webkit-scrollbar-thumb {"
         << "  background: rgba(255, 255, 255, 0.2);"
         << "  border-radius: 4px;"
         << "}"
         << ".breadcrumb-item {"
         << "  display: flex;"
         << "  align-items: center;"
         << "  color: var(--text-color);"
         << "  opacity: 0.7;"
         << "  text-decoration: none;"
         << "  padding: 0.25rem 0.5rem;"
         << "  border-radius: 4px;"
         << "}"
         << ".breadcrumb-item:hover {"
         << "  background: rgba(255, 255, 255, 0.1);"
         << "  opacity: 1;"
         << "}"
         << ".breadcrumb-item.active {"
         << "  opacity: 1;"
         << "  font-weight: 500;"
         << "}"
         << ".breadcrumb-separator {"
         << "  opacity: 0.5;"
         << "  margin: 0 0.25rem;"
         << "}"
         << ".view-options {"
         << "  display: flex;"
         << "  align-items: center;"
         << "  gap: 0.5rem;"
         << "}"
         << ".container {"
         << "  width: 95%;"
         << "  max-width: 1400px;"
         << "  margin: 1rem auto;"
         << "  animation: fadeIn 0.5s ease-out;"
         << "}"
         << ".warning-banner {"
         << "  background-color: var(--warning-bg);"
         << "  border: 1px solid var(--warning-border);"
         << "  color: var(--warning-text);"
         << "  padding: 1rem;"
         << "  margin-bottom: 1rem;"
         << "  border-radius: 8px;"
         << "  animation: pulse 2s infinite;"
         << "}"
         << ".warning-banner h3 {"
         << "  margin-bottom: 0.5rem;"
         << "  display: flex;"
         << "  align-items: center;"
         << "  gap: 0.5rem;"
         << "}"
         << ".warning-banner p {"
         << "  margin: 0.25rem 0;"
         << "}"
         << ".explorer {"
         << "  background: var(--card-bg);"
         << "  border-radius: 8px;"
         << "  overflow: hidden;"
         << "  box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2);"
         << "}"
         << ".explorer-body {"
         << "  padding: 0.5rem;"
         << "}"
         << ".file-grid {"
         << "  display: grid;"
         << "  grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));"
         << "  gap: 1rem;"
         << "  padding: 0.5rem;"
         << "}"
         << ".file-item {"
         << "  display: flex;"
         << "  flex-direction: column;"
         << "  align-items: center;"
         << "  text-align: center;"
         << "  padding: 0.75rem;"
         << "  border-radius: 8px;"
         << "  transition: background-color 0.2s;"
         << "  cursor: pointer;"
         << "  text-decoration: none;"
         << "  color: var(--text-color);"
         << "}"
         << ".file-item:hover {"
         << "  background-color: var(--card-hover);"
         << "}"
         << ".file-icon {"
         << "  width: 48px;"
         << "  height: 48px;"
         << "  margin-bottom: 0.5rem;"
         << "  display: flex;"
         << "  align-items: center;"
         << "  justify-content: center;"
         << "}"
         << ".file-icon svg {"
         << "  width: 100%;"
         << "  height: 100%;"
         << "}"
         << ".file-name {"
         << "  font-size: 0.9rem;"
         << "  word-break: break-word;"
         << "  max-width: 100%;"
         << "  overflow: hidden;"
         << "  text-overflow: ellipsis;"
         << "  display: -webkit-box;"
         << "  -webkit-line-clamp: 2;"
         << "  -webkit-box-orient: vertical;"
         << "}"
         << ".footer {"
         << "  margin-top: 2rem;"
         << "  text-align: center;"
         << "  font-size: 0.85rem;"
         << "  color: rgba(255, 255, 255, 0.5);"
         << "}"
         << "@keyframes fadeIn {"
         << "  from { opacity: 0; transform: translateY(10px); }"
         << "  to { opacity: 1; transform: translateY(0); }"
         << "}"
         << "@keyframes pulse {"
         << "  0% { opacity: 1; }"
         << "  50% { opacity: 0.8; }"
         << "  100% { opacity: 1; }"
         << "}"
         << "@media (max-width: 768px) {"
         << "  .file-grid {"
         << "    grid-template-columns: repeat(auto-fill, minmax(100px, 1fr));"
         << "  }"
         << "  header {"
         << "    padding: 0.5rem;"
         << "  }"
         << "  .breadcrumb {"
         << "    margin-left: 0.5rem;"
         << "  }"
         << "}"
         << "</style></head><body>";

    // Header with navigation buttons
    html << "<header>"
         << "  <div class=\"navigation\">"
         << "    <button class=\"nav-button\" onclick=\"history.back()\">"
         << "      <svg viewBox=\"0 0 24 24\"><path d=\"M20,11V13H8L13.5,18.5L12.08,19.92L4.16,12L12.08,4.08L13.5,5.5L8,11H20Z\"></path></svg>"
         << "    </button>"
         << "    <button class=\"nav-button\" onclick=\"history.forward()\">"
         << "      <svg viewBox=\"0 0 24 24\"><path d=\"M4,11V13H16L10.5,18.5L11.92,19.92L19.84,12L11.92,4.08L10.5,5.5L16,11H4Z\"></path></svg>"
         << "    </button>"
         << "  </div>"
         << "  <div class=\"breadcrumb\">";

    // Build breadcrumb navigation
    std::string currentPath = "";
    std::vector<std::string> pathParts;
    std::string pathCopy = path;
    
    // Handle root directory
    if (path == "." || path == "./") {
        html << "<a href=\"/\" class=\"breadcrumb-item active\">"
             << "  <svg style=\"width:20px;height:20px;margin-right:4px;\" viewBox=\"0 0 24 24\">"
             << "    <path fill=\"currentColor\" d=\"M10,20V14H14V20H19V12H22L12,3L2,12H5V20H10Z\" />"
             << "  </svg>"
             << "  Root"
             << "</a>";
    } else {
        html << "<a href=\"/\" class=\"breadcrumb-item\">"
             << "  <svg style=\"width:20px;height:20px;margin-right:4px;\" viewBox=\"0 0 24 24\">"
             << "    <path fill=\"currentColor\" d=\"M10,20V14H14V20H19V12H22L12,3L2,12H5V20H10Z\" />"
             << "  </svg>"
             << "  Root"
             << "</a>"
             << "<span class=\"breadcrumb-separator\">/</span>";
        
        // Split path into parts
        if (pathCopy.front() == '.') pathCopy = pathCopy.substr(1);
        if (pathCopy.front() == '/') pathCopy = pathCopy.substr(1);
        if (pathCopy.back() == '/') pathCopy = pathCopy.substr(0, pathCopy.size() - 1);
        
        std::istringstream pathStream(pathCopy);
        std::string part;
        while (std::getline(pathStream, part, '/')) {
            if (!part.empty()) {
                pathParts.push_back(part);
            }
        }
        
        // Build breadcrumb items
        for (size_t i = 0; i < pathParts.size(); ++i) {
            currentPath += "/" + pathParts[i];
            bool isLast = (i == pathParts.size() - 1);
            
            html << "<a href=\"" << currentPath << "\" class=\"breadcrumb-item " << (isLast ? "active" : "") << "\">"
                 << pathParts[i]
                 << "</a>";
            
            if (!isLast) {
                html << "<span class=\"breadcrumb-separator\">/</span>";
            }
        }
    }
    
    html << "  </div>"
         << "  <div class=\"view-options\">"
         << "    <button class=\"nav-button\" title=\"Grid View\">"
         << "      <svg viewBox=\"0 0 24 24\"><path d=\"M3,3H11V11H3V3M3,13H11V21H3V13M13,3H21V11H13V3M13,13H21V21H13V13Z\"></path></svg>"
         << "    </button>"
         << "    <button class=\"nav-button\" title=\"List View\">"
         << "      <svg viewBox=\"0 0 24 24\"><path d=\"M3,4H21V8H3V4M3,10H21V14H3V10M3,16H21V20H3V16Z\"></path></svg>"
         << "    </button>"
         << "  </div>"
         << "</header>"
         << "<div class=\"container\">";
    
    // Show warning banner if no index.html exists
    if (!fs::exists("./index.html") && !fs::exists("./index.htm")) {
        html << "<div class=\"warning-banner\">"
             << "  <h3>"
             << "    <svg style=\"width:24px;height:24px\" viewBox=\"0 0 24 24\">"
             << "      <path fill=\"currentColor\" d=\"M13,13H11V7H13M13,17H11V15H13M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z\" />"
             << "    </svg>"
             << "    MTWS Explorer Component"
             << "  </h3>"
             << "  <p>This is the built-in file explorer of My Tiny Web Server.</p>"
             << "  <p>No index.html or index.htm file was found in this directory.</p>"
             << "</div>";
    }
    
    html << "<div class=\"explorer\">"
         << "  <div class=\"explorer-body\">"
         << "    <div class=\"file-grid\">";

    // Add parent directory link if not in root
    if (path != "." && path != "./") {
        html << "<a href=\"..\" class=\"file-item\">"
             << "  <div class=\"file-icon\">"
             << "    <svg viewBox=\"0 0 24 24\">"
             << "      <path fill=\"#8a85ff\" d=\"M20,18H4V8H20M20,6H12L10,4H4C2.89,4 2,4.89 2,6V18A2,2 0 0,0 4,20H20A2,2 0 0,0 22,18V8C22,6.89 21.1,6 20,6Z\" />"
             << "    </svg>"
             << "  </div>"
             << "  <div class=\"file-name\">..</div>"
             << "</a>";
    }

    // Sort entries: directories first, then files
    std::vector<fs::directory_entry> dirs;
    std::vector<fs::directory_entry> files;
    
    for (const auto& entry : fs::directory_iterator(path)) {
        if (fs::is_directory(entry)) {
            dirs.push_back(entry);
        } else {
            files.push_back(entry);
        }
    }
    
    // Add directories
    for (const auto& entry : dirs) {
        std::string name = entry.path().filename().string();
        
        html << "<a href=\"" << name << "\" class=\"file-item\">"
             << "  <div class=\"file-icon\">"
             << "    <svg viewBox=\"0 0 24 24\">"
             << "      <path fill=\"#8a85ff\" d=\"M20,18H4V8H20M20,6H12L10,4H4C2.89,4 2,4.89 2,6V18A2,2 0 0,0 4,20H20A2,2 0 0,0 22,18V8C22,6.89 21.1,6 20,6Z\" />"
             << "    </svg>"
             << "  </div>"
             << "  <div class=\"file-name\">" << name << "</div>"
             << "</a>";
    }
    
    // Add files
    for (const auto& entry : files) {
        std::string name = entry.path().filename().string();
        std::string iconColor;
        std::string iconSvg = getFileIconSvg(name, iconColor);
        
        html << "<a href=\"" << name << "\" class=\"file-item\">"
             << "  <div class=\"file-icon\">"
             << "    <svg viewBox=\"0 0 24 24\">"
             << "      <path fill=\"" << iconColor << "\" d=\"" << iconSvg << "\" />"
             << "    </svg>"
             << "  </div>"
             << "  <div class=\"file-name\">" << name << "</div>"
             << "</a>";
    }
    
    html << "    </div>"
         << "  </div>"
         << "</div>"
         << "<div class=\"footer\">"
         << "  Powered by My Tiny Web Server (MTWS)"
         << "</div>"
         << "</div></body></html>";
    
    return html.str();
}

bool checkPortAvailable(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    int result = bind(sock, (struct sockaddr*)&address, sizeof(address));
    close(sock);
    
    return result >= 0;
}

bool tryStartServer(int port, int& server_fd) {
    sockaddr_in address{};
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        return false;
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        close(server_fd);
        return false;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_fd);
        return false;
    }
    
    return true;
}

// Function to get the server's IP address
std::string getLocalIP() {
    char hostbuffer[256];
    if (gethostname(hostbuffer, sizeof(hostbuffer)) != 0) {
        return "127.0.0.1";
    }
    
    struct hostent* host_entry = gethostbyname(hostbuffer);
    if (host_entry == nullptr) {
        return "127.0.0.1";
    }
    
    char* ip = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    return std::string(ip);
}

// Console input handler
void consoleHandler() {
    while (g_running) {
        std::cout << "\033[32mMTWS> \033[0m";
        std::string command;
        std::getline(std::cin, command);
        
        if (command == "stop") {
            log(LogLevel::INFO, "Server shutdown requested via console");
            g_running = false;
            g_cv.notify_all();
            break;
        } else if (command == "ip") {
            std::string ip = getLocalIP();
            log(LogLevel::INFO, "Server running at http://" + ip + ":" + std::to_string(g_port));
        } else if (command == "plugins") {
            if (g_plugins.empty()) {
                log(LogLevel::INFO, "No plugins loaded");
            } else {
                log(LogLevel::INFO, "Active plugins:");
                for (const auto& plugin : g_plugins) {
                    log(LogLevel::INFO, "  - " + plugin);
                }
            }
        } else if (command.substr(0, 13) == "plugins stop ") {
            std::string pluginName = command.substr(13);
            bool found = false;
            for (auto it = g_plugins.begin(); it != g_plugins.end(); ++it) {
                if (*it == pluginName) {
                    g_plugins.erase(it);
                    log(LogLevel::INFO, "Plugin '" + pluginName + "' stopped");
                    found = true;
                    break;
                }
            }
            if (!found) {
                log(LogLevel::ERROR, "Plugin '" + pluginName + "' not found");
            }
        } else if (command == "restart") {
            log(LogLevel::INFO, "Server restart requested");
            g_restart = true;
            g_running = false;
            g_cv.notify_all();
            break;
        } else if (command == "restart --force") {
            log(LogLevel::INFO, "Server force restart requested");
            g_restart = true;
            g_running = false;
            g_cv.notify_all();
            break;
        } else if (!command.empty()) {
            log(LogLevel::ERROR, "Unknown command: " + command);
            log(LogLevel::INFO, "Available commands: ip, stop, plugins, plugins stop <plugin>, restart, restart --force");
        }
    }
}

// Server main function
void runServer(int port) {
    g_port = port;
    int server_fd;
    
    if (!tryStartServer(port, server_fd)) {
        log(LogLevel::FATAL, "Failed to start server on port " + std::to_string(port));
        return;
    }
    
    // Check for index.html at startup
    if (!fs::exists("./index.html") && !fs::exists("./index.htm")) {
        log(LogLevel::WARN, "No 'index.html' in this directory found; explorer will be shown instead");
    }
    
    std::string ip = getLocalIP();
    log(LogLevel::INFO, "Server started on port " + std::to_string(port));
    log(LogLevel::INFO, "Go to http://" + ip + ":" + std::to_string(port));
    
    // Start console handler in a separate thread
    std::thread consoleThread(consoleHandler);
    consoleThread.detach();
    
    while (g_running) {
        // Set socket to non-blocking mode to allow checking g_running
        int flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
        
        if (listen(server_fd, 10) < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log(LogLevel::ERROR, "Listen failed");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        sockaddr_in client;
        socklen_t client_len = sizeof(client);
        int client_fd = accept(server_fd, (struct sockaddr*)&client, &client_len);
        
        if (client_fd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log(LogLevel::ERROR, "Accept failed");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Set client socket back to blocking mode for read/write
        flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags & ~O_NONBLOCK);
        
        char buffer[4096] = {0};
        ssize_t bytes_read = read(client_fd, buffer, 4096);
        
        if (bytes_read <= 0) {
            close(client_fd);
            continue;
        }
        
        std::string request(buffer);

        std::smatch match;
        std::regex_search(request, match, std::regex("GET (/[^ ]*)"));
        std::string path = match.size() > 1 ? match[1].str() : "/";
        
        // URL decode the path
        std::string decoded_path;
        for (size_t i = 0; i < path.length(); ++i) {
            if (path[i] == '%' && i + 2 < path.length()) {
                int value;
                std::istringstream is(path.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    decoded_path += static_cast<char>(value);
                    i += 2;
                } else {
                    decoded_path += path[i];
                }
            } else if (path[i] == '+') {
                decoded_path += ' ';
            } else {
                decoded_path += path[i];
            }
        }
        path = decoded_path;
        
        if (path == "/") path = "/index.html";

        std::string filePath = "." + path;
        std::string response;

        if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
            // Serve the file
            std::ifstream file(filePath, std::ios::binary);
            if (!file) {
                std::string errorMsg = "<h1>500 Internal Server Error</h1><p>Could not open file: " + path + "</p>";
                response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: " + 
                           std::to_string(errorMsg.size()) + "\r\n\r\n" + errorMsg;
            } else {
                std::ostringstream content;
                content << file.rdbuf();
                std::string body = content.str();
                response = "HTTP/1.1 200 OK\r\nContent-Type: " + getMimeType(filePath) + 
                           "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
            }
        } else if (fs::exists(filePath) && fs::is_directory(filePath)) {
            // Check for index.html in the directory
            std::string indexPath = filePath + "/index.html";
            std::string indexPathHtm = filePath + "/index.htm";
            
            if (fs::exists(indexPath) && fs::is_regular_file(indexPath)) {
                std::ifstream file(indexPath, std::ios::binary);
                std::ostringstream content;
                content << file.rdbuf();
                std::string body = content.str();
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + 
                           std::to_string(body.size()) + "\r\n\r\n" + body;
            } else if (fs::exists(indexPathHtm) && fs::is_regular_file(indexPathHtm)) {
                std::ifstream file(indexPathHtm, std::ios::binary);
                std::ostringstream content;
                content << file.rdbuf();
                std::string body = content.str();
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + 
                           std::to_string(body.size()) + "\r\n\r\n" + body;
            } else {
                // Generate directory listing
                std::string body = generateExplorerHTML(filePath);
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + 
                           std::to_string(body.size()) + "\r\n\r\n" + body;
            }
        } else {
            if (path == "/index.html" || path == "/index.htm") {
                std::string body = generateExplorerHTML(".");
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + 
                           std::to_string(body.size()) + "\r\n\r\n" + body;
            } else {
                std::string notFound = "<html><head><title>404 Not Found</title><style>body{font-family:system-ui;background:#121212;color:#f0f0f0;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;flex-direction:column;}.container{text-align:center;animation:fadeIn 0.5s ease-out;}h1{color:#ff5577;font-size:3rem;margin-bottom:1rem;}p{font-size:1.2rem;opacity:0.8;}@keyframes fadeIn{from{opacity:0;transform:translateY(-20px);}to{opacity:1;transform:translateY(0);}}</style></head><body><div class='container'><h1>404 Not Found</h1><p>The requested resource could not be found on this server.</p></div></body></html>";
                response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: " + 
                           std::to_string(notFound.size()) + "\r\n\r\n" + notFound;
            }
        }
        
        send(client_fd, response.c_str(), response.size(), 0);
        close(client_fd);
    }
    
    close(server_fd);
    log(LogLevel::INFO, "Server stopped");
}

int main(int argc, char* argv[]) {
    // Register signal handler for Ctrl+C
    signal(SIGINT, signalHandler);
    
    int port = 80;
    bool port_specified = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-p") {
            if (i + 1 < argc) {
                try {
                    port = std::stoi(argv[i + 1]);
                    port_specified = true;
                    i++; // Skip the port number in the next iteration
                } catch (const std::exception& e) {
                    log(LogLevel::FATAL, "Invalid port number specified");
                }
            } else {
                log(LogLevel::FATAL, "Port flag (-p) used but no port specified");
            }
        }
    }
    
    // Check if the specified port is available
    if (port_specified && !checkPortAvailable(port)) {
        log(LogLevel::FATAL, "Port " + std::to_string(port) + " is already in use or unavailable");
    }
    
    // Main server loop with restart capability
    do {
        g_running = true;
        g_restart = false;
        
        runServer(port);
        
        if (g_restart) {
            log(LogLevel::INFO, "Restarting server...");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } while (g_restart);
    
    return 0;
}
