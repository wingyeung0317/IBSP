#!/bin/bash
# ============================================================================
# Load Environment Variables Helper Script
# ============================================================================
# This script loads environment variables from .env file
# Usage: source load-env.sh

# Check if .env file exists
if [ ! -f ".env" ]; then
    echo "⚠️  Warning: .env file not found!"
    echo "   Copy from .env.example: cp .env.example .env"
    return 1
fi

# Load environment variables from .env file
set -a  # Automatically export all variables
source .env
set +a

# Display loaded configuration
echo "✅ Environment variables loaded from .env"
echo ""
echo "Configuration:"
echo "  SERVER_IP=$SERVER_IP"
echo "  API_PORT=$API_PORT"
echo "  DASHBOARD_PORT=$DASHBOARD_PORT"
echo ""
echo "You can now use these variables in your scripts."
echo "Example: curl http://\$SERVER_IP:\$API_PORT/health"
