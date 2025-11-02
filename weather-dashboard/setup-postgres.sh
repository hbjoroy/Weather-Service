#!/bin/bash
# Setup PostgreSQL database and user for Weather Dashboard
# This script will guide you through creating the database on your cluster

set -e

echo "=========================================="
echo "Weather Dashboard PostgreSQL Setup"
echo "=========================================="
echo ""

# Configuration
POSTGRES_HOST="postgres-postgresql.database.svc.cluster.local"
POSTGRES_PORT="5432"
DB_NAME="bjosoft-weather"
DB_USER="weather"
NAMESPACE="database"
SERVICE="postgres-postgresql"

echo "This script will:"
echo "  1. Connect to PostgreSQL in your cluster"
echo "  2. Create database: $DB_NAME"
echo "  3. Create user: $DB_USER"
echo "  4. Apply the schema"
echo ""

# Ask for PostgreSQL admin password
echo "Enter the PostgreSQL admin password (postgres user):"
read -s POSTGRES_PASSWORD
echo ""

# Generate a secure password for the weather user
echo "Generating secure password for 'weather' user..."
WEATHER_PASSWORD=$(openssl rand -base64 32 | tr -d "=+/" | cut -c1-25)
echo "Generated password: $WEATHER_PASSWORD"
echo "(Save this password - you'll need it for the Kubernetes secret)"
echo ""

# Create a temporary pod to run psql commands
echo "Creating temporary PostgreSQL client pod..."
kubectl run -it --rm postgres-setup-client \
  --image=postgres:16-alpine \
  --restart=Never \
  -n $NAMESPACE \
  --env="PGPASSWORD=$POSTGRES_PASSWORD" \
  --command -- sh -c "

echo '=========================================='
echo 'Connecting to PostgreSQL server...'
echo '=========================================='

# Test connection
if ! psql -h $SERVICE -U postgres -c 'SELECT version();' > /dev/null 2>&1; then
  echo 'ERROR: Could not connect to PostgreSQL'
  echo 'Please check that the password is correct'
  exit 1
fi

echo 'Connected successfully!'
echo ''

# Create database
echo 'Creating database: $DB_NAME'
psql -h $SERVICE -U postgres -c \"CREATE DATABASE \\\"$DB_NAME\\\" ENCODING 'UTF8' LC_COLLATE='en_US.UTF-8' LC_CTYPE='en_US.UTF-8' TEMPLATE=template0;\" 2>&1 | grep -v 'already exists' || echo 'Database already exists (OK)'

# Create user
echo 'Creating user: $DB_USER'
psql -h $SERVICE -U postgres -c \"CREATE USER $DB_USER WITH PASSWORD '$WEATHER_PASSWORD';\" 2>&1 | grep -v 'already exists' || echo 'User already exists (OK)'

# Grant privileges
echo 'Granting privileges...'
psql -h $SERVICE -U postgres -c \"GRANT ALL PRIVILEGES ON DATABASE \\\"$DB_NAME\\\" TO $DB_USER;\"
psql -h $SERVICE -U postgres -d $DB_NAME -c \"GRANT ALL ON SCHEMA public TO $DB_USER;\"
psql -h $SERVICE -U postgres -d $DB_NAME -c \"ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT ALL ON TABLES TO $DB_USER;\"

echo ''
echo 'Database and user created successfully!'
echo ''
"

echo ""
echo "=========================================="
echo "Applying database schema..."
echo "=========================================="

# Apply schema using the weather user
SCHEMA_SQL='
-- Weather Dashboard User Profiles Schema
CREATE TABLE IF NOT EXISTS user_profiles (
    user_id VARCHAR(128) PRIMARY KEY,
    profile_data JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_user_profiles_updated_at ON user_profiles(updated_at);

CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language '\''plpgsql'\'';

DROP TRIGGER IF EXISTS update_user_profiles_updated_at ON user_profiles;
CREATE TRIGGER update_user_profiles_updated_at 
    BEFORE UPDATE ON user_profiles 
    FOR EACH ROW 
    EXECUTE FUNCTION update_updated_at_column();
'

kubectl run -it --rm postgres-schema-apply \
  --image=postgres:16-alpine \
  --restart=Never \
  -n $NAMESPACE \
  --env="PGPASSWORD=$WEATHER_PASSWORD" \
  --command -- sh -c "
psql -h $SERVICE -U $DB_USER -d $DB_NAME <<'EOF'
$SCHEMA_SQL
EOF
"

echo ""
echo "=========================================="
echo "Setup Complete!"
echo "=========================================="
echo ""
echo "Database: $DB_NAME"
echo "User: $DB_USER"
echo "Password: $WEATHER_PASSWORD"
echo ""
echo "Next steps:"
echo ""
echo "1. Create Kubernetes secret with the password:"
echo ""
echo "   kubectl create secret generic postgres-credentials \\"
echo "     --from-literal=database-user=$DB_USER \\"
echo "     --from-literal=database-password='$WEATHER_PASSWORD' \\"
echo "     -n weather"
echo ""
echo "2. Verify the connection:"
echo ""
echo "   kubectl run -it --rm postgres-test \\"
echo "     --image=postgres:16-alpine \\"
echo "     --restart=Never \\"
echo "     --env=PGPASSWORD='$WEATHER_PASSWORD' \\"
echo "     -- psql -h $POSTGRES_HOST -U $DB_USER -d $DB_NAME"
echo ""
echo "3. Deploy the weather dashboard with Helm"
echo ""
