#-----------------------------------------------------------------------------
# Q2NS - Quantum Network Simulator
# Copyright (c) 2026 quantuminternet.it
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#---------------------------------------------------------------------------*/

#!/usr/bin/env bash
#
# Q2NSViz Server - HTTP server for quantum visualization
# Usage: ./q2nsviz-serve.sh [PORT] [DIRECTORY]
#
set -euo pipefail

# =============================================================================
# Configuration
# =============================================================================
readonly PORT="${1:-8000}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly DIR="${2:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
readonly URL="http://localhost:$PORT/tools/q2nsviz/viewer.html"

# Global variable for server PID
SERVER_PID=""

# =============================================================================
# Utility Functions
# =============================================================================

log() {
    echo "[q2nsviz] $1"
}

is_port_busy() {
    lsof -i ":$1" >/dev/null 2>&1
}

get_port_processes() {
    lsof -t -i ":$1" 2>/dev/null || true
}

# =============================================================================
# Port Management
# =============================================================================

kill_port_processes() {
    local port="$1"
    log "Checking for existing processes on port $port..."
    
    local pids
    pids=$(get_port_processes "$port")
    
    if [ -n "$pids" ]; then
        log "Found existing processes: $pids"
        log "Terminating processes..."
        
        # Graceful termination
        kill $pids 2>/dev/null || true
        sleep 1
        
        # Force kill if still running
        kill -9 $pids 2>/dev/null || true
        sleep 1
        
        log "Processes terminated"
    else
        log "No existing processes found on port $port"
    fi
}

handle_port_conflict() {
    local port="$1"
    
    if ! is_port_busy "$port"; then
        return 0
    fi
    
    log "Warning: Port $port is already in use!"
    echo -n "[q2nsviz] Kill existing process and continue? (y/N): "
    read -n 1 -r reply
    echo
    
    if [[ $reply =~ ^[Yy]$ ]]; then
        kill_port_processes "$port"
        return 0
    else
        log "Aborted. Try with a different port: ./tools/q2nsviz-serve.sh <port>"
        return 1
    fi
}

# =============================================================================
# Server Management
# =============================================================================

cleanup() {
    echo
    log "Cleaning up..."
    
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        log "Stopping server (PID: $SERVER_PID)"
        
        # Graceful shutdown
        kill "$SERVER_PID" 2>/dev/null || true
        sleep 1
        
        # Force kill if still running
        if kill -0 "$SERVER_PID" 2>/dev/null; then
            log "Force stopping server..."
            kill -9 "$SERVER_PID" 2>/dev/null || true
        fi
    fi
    
    log "Cleanup complete"
}

start_server() {
    local port="$1"
    local directory="$2"
    
    log "Starting server on port $port..."
    log "Serving directory: $directory"
    
    # Start HTTP server in background
    ( cd "$directory" && python3 -m http.server "$port" ) &
    SERVER_PID=$!
    
    # Set up cleanup handlers
    trap cleanup EXIT INT TERM
    
    # Verify server started
    sleep 1
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        log "Error: Failed to start server on port $port"
        return 1
    fi
    
    log "Server started successfully (PID: $SERVER_PID)"
    return 0
}

open_browser() {
    local url="$1"
    
    if command -v open >/dev/null 2>&1; then
        open "$url"
    elif command -v xdg-open >/dev/null 2>&1; then
        xdg-open "$url"
    else
        log "Please open $url in your browser"
        return
    fi
    
    log "Opening $url in browser"
}

# =============================================================================
# Main Execution
# =============================================================================

main() {
    log "Q2NSViz Server starting..."
    log "Configuration: Port=$PORT, Directory=$DIR"
    
    # Handle port conflicts
    if ! handle_port_conflict "$PORT"; then
        exit 1
    fi
    
    # Start the server
    if ! start_server "$PORT" "$DIR"; then
        exit 1
    fi
    
    # Open browser
    open_browser "$URL"
    
    # Wait for server
    log "Server running at $URL"
    log "Press Ctrl+C to stop the server"
    wait "$SERVER_PID"
}

# Run main function
main "$@"
