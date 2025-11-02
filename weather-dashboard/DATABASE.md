# PostgreSQL Database Setup for Weather Dashboard

## Database Schema

The weather dashboard uses PostgreSQL to store user profiles persistently.

### Table Structure

```sql
CREATE TABLE user_profiles (
    user_id VARCHAR(128) PRIMARY KEY,
    profile_data JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
```

### Profile Data (JSONB)

```json
{
  "name": "User Name",
  "tempUnit": "celsius",
  "windUnit": "ms",
  "defaultLocation": "Paros",
  "isAuthenticated": true
}
```

## Local Development (Docker)

Start a PostgreSQL container for local development:

```bash
docker run -d \
  --name weather-postgres \
  -e POSTGRES_DB=bjosoft-weather \
  -e POSTGRES_USER=weather \
  -e POSTGRES_PASSWORD=weather123 \
  -p 5432:5432 \
  postgres:16-alpine
```

Apply the schema:

```bash
docker exec -i weather-postgres psql -U weather -d bjosoft-weather < backend/schema.sql
```

Run the dashboard server:

```bash
cd backend
DATABASE_URL="host=localhost port=5432 dbname=bjosoft-weather user=weather password=weather123 sslmode=disable" \
  ./build/weather-dashboard-server
```

## Kubernetes Production Setup

### 1. Create Database Secret

Create a Kubernetes secret with your PostgreSQL credentials:

```bash
kubectl create secret generic postgres-credentials \
  --from-literal=database-user=weather \
  --from-literal=database-password=YOUR_SECURE_PASSWORD \
  -n weather
```

Or use the example file (after updating password):

```bash
kubectl apply -f helm/weather-stack/postgres-secret-example.yaml
```

### 2. Update Helm Values

In `values.yaml`:

```yaml
weatherDashboard:
  env:
    databaseHost: "postgresql.default.svc.cluster.local"
    databasePort: "5432"
    databaseName: "bjosoft-weather"
    databaseSslMode: "require"
  
  existingDatabaseSecret: "postgres-credentials"
```

### 3. Apply Database Schema

Connect to your PostgreSQL instance and run:

```bash
kubectl run -it --rm postgres-client \
  --image=postgres:16-alpine \
  --restart=Never \
  -- psql -h postgresql.default.svc.cluster.local \
         -U weather \
         -d bjosoft-weather \
         -f /path/to/schema.sql
```

Or copy the schema into the pod:

```bash
kubectl cp backend/schema.sql postgres-client:/tmp/schema.sql
```

### 4. Deploy with Helm

```bash
helm upgrade --install weather-stack ./helm/weather-stack \
  --namespace weather \
  --create-namespace
```

## Environment Variables

The dashboard supports two methods for database configuration:

### Method 1: Full Connection String (DATABASE_URL)

```bash
DATABASE_URL="host=localhost port=5432 dbname=bjosoft-weather user=weather password=secret sslmode=require"
```

### Method 2: Separate Components (Recommended for Kubernetes)

```bash
DATABASE_HOST=postgresql.default.svc.cluster.local
DATABASE_PORT=5432
DATABASE_NAME=bjosoft-weather
DATABASE_USER=weather
DATABASE_PASSWORD=secret
DATABASE_SSLMODE=require
```

Method 2 is preferred in Kubernetes as it allows using secrets for credentials while keeping other values in ConfigMaps.

## Verifying Database Connection

Check the logs after deployment:

```bash
kubectl logs -n weather deployment/weather-stack-dashboard -f
```

You should see:

```
Database connected successfully
PostgreSQL server version: 160010
Session manager initialized with default profile
```

## Troubleshooting

### Connection refused

Check that the database host and port are correct:

```bash
kubectl run -it --rm postgres-test \
  --image=postgres:16-alpine \
  --restart=Never \
  -- psql -h postgresql.default.svc.cluster.local \
         -U weather \
         -d bjosoft-weather
```

### Authentication failed

Verify the secret contains correct credentials:

```bash
kubectl get secret postgres-credentials -n weather -o jsonpath='{.data.database-user}' | base64 -d
kubectl get secret postgres-credentials -n weather -o jsonpath='{.data.database-password}' | base64 -d
```

### SSL/TLS errors

If using self-signed certificates, set `sslmode=require` instead of `verify-full`.

For development/testing without SSL:

```yaml
weatherDashboard:
  env:
    databaseSslMode: "disable"
```

## Database Maintenance

### Backup

```bash
kubectl exec -it postgresql-0 -n default -- \
  pg_dump -U weather bjosoft-weather > backup.sql
```

### View all profiles

```bash
kubectl exec -it postgresql-0 -n default -- \
  psql -U weather -d bjosoft-weather \
  -c "SELECT user_id, profile_data FROM user_profiles;"
```

### Clear test data

```bash
kubectl exec -it postgresql-0 -n default -- \
  psql -U weather -d bjosoft-weather \
  -c "DELETE FROM user_profiles WHERE user_id LIKE 'test%';"
```
