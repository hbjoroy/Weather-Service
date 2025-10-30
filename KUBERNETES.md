# Kubernetes Deployment Guide

This guide covers deploying the Weather Service stack to a Kubernetes cluster using Helm.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Docker Images](#docker-images)
- [Helm Chart](#helm-chart)
- [Quick Start](#quick-start)
- [Configuration](#configuration)
- [Production Deployment](#production-deployment)
- [Troubleshooting](#troubleshooting)

## Prerequisites

- Kubernetes cluster (v1.20+)
- Helm 3.x installed
- kubectl configured to access your cluster
- Docker for building images
- Container registry (Docker Hub, GCR, ECR, etc.)

## Docker Images

### Multi-Platform Support

The Docker images support both **amd64** and **arm64** architectures, making them suitable for deployment on:
- x86_64 servers (Intel/AMD)
- ARM-based servers (AWS Graviton, Ampere, etc.)
- Apple Silicon (M1/M2/M3) for local development
- Raspberry Pi and other ARM devices

### Building Multi-Platform Images

**For CI/CD and Registry Push:**

```bash
# Set your registry and version
export DOCKER_REGISTRY=hbjoroy  # or your registry
export VERSION=1.0.0

# Build and push for both amd64 and arm64
./build-images.sh
```

This uses Docker Buildx to create manifest lists supporting both platforms.

**For Local Development:**

```bash
# Build for your current platform only
./build-local.sh
```

**Manual Multi-Platform Build:**

```bash
# Create and use buildx builder
docker buildx create --name weather-builder --use
docker buildx inspect --bootstrap

# Build Weather Service for both platforms
cd weather
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --tag your-registry/weather-service:1.0.0 \
  --tag your-registry/weather-service:latest \
  --push \
  .

# Build Weather Dashboard for both platforms
cd ../weather-dashboard
# Build frontend first
cd frontend && npm install && npm run build && cd ..
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --tag your-registry/weather-dashboard:1.0.0 \
  --tag your-registry/weather-dashboard:latest \
  --push \
  .
```

### Inspecting Multi-Platform Images

```bash
# View supported platforms
docker buildx imagetools inspect hbjoroy/weather-service:1.0.0

# Output shows:
# Name:      docker.io/hbjoroy/weather-service:1.0.0
# MediaType: application/vnd.docker.distribution.manifest.list.v2+json
# Platforms: linux/amd64, linux/arm64
```

### Single-Platform Build

If you only need one platform:

```bash
# Build Weather Service API
cd weather
docker build -t your-registry/weather-service:1.0.0 .
docker push your-registry/weather-service:1.0.0

# Build Weather Dashboard
cd ../weather-dashboard
# First build the frontend
cd frontend && npm install && npm run build
cd ..
docker build -t your-registry/weather-dashboard:1.0.0 .
docker push your-registry/weather-dashboard:1.0.0
```

### Image Details

**Weather Service** (`weather/Dockerfile`):
- Multi-stage build for minimal image size
- Based on Debian 12 slim
- Runs as non-root user (UID 1000)
- Includes health check
- Exposes port 8080

**Weather Dashboard** (`weather-dashboard/Dockerfile`):
- Multi-stage build
- Serves static frontend files
- Runs as non-root user
- Exposes port 3001

## Helm Chart

The Helm chart is located in `helm/weather-stack/` and includes:

### Components

1. **Weather Service API**
   - Deployment with configurable replicas
   - ClusterIP Service
   - Ingress (optional)
   - Horizontal Pod Autoscaler
   - Secrets for API keys

2. **Weather Dashboard**
   - Deployment with configurable replicas
   - ClusterIP Service
   - Ingress (optional)
   - Horizontal Pod Autoscaler

### Chart Structure

```
helm/weather-stack/
├── Chart.yaml              # Chart metadata
├── values.yaml             # Default configuration
└── templates/
    ├── _helpers.tpl        # Template helpers
    ├── weather-service-deployment.yaml
    ├── weather-service-service.yaml
    ├── weather-service-ingress.yaml
    ├── weather-service-hpa.yaml
    ├── weather-service-secret.yaml
    ├── weather-dashboard-deployment.yaml
    ├── weather-dashboard-service.yaml
    └── weather-dashboard-ingress.yaml
```

## Quick Start

### 1. Create Namespace

```bash
kubectl create namespace weather
```

### 2. Create Secrets

Create a `secrets.yaml` file (don't commit this!):

```bash
kubectl create secret generic weather-secrets \
  --from-literal=WEATHERAPI_KEY=your_weatherapi_key \
  --from-literal=SLACK_BOT_TOKEN=xoxb-your-slack-token \
  --from-literal=SLACK_APP_ID=A01234567 \
  -n weather
```

### 3. Install with Helm

```bash
helm install weather-stack ./helm/weather-stack \
  --namespace weather \
  --set weatherService.image.repository=your-registry/weather-service \
  --set weatherService.image.tag=1.0.0 \
  --set weatherDashboard.image.repository=your-registry/weather-dashboard \
  --set weatherDashboard.image.tag=1.0.0 \
  --set weatherService.existingSecret=weather-secrets
```

### 4. Access the Services

```bash
# Port-forward Weather Service API
kubectl port-forward -n weather svc/weather-stack-service 8080:8080

# Port-forward Dashboard
kubectl port-forward -n weather svc/weather-stack-dashboard 3001:3001

# Access locally
curl http://localhost:8080/health
open http://localhost:3001
```

## Configuration

### Basic Configuration

Create a `custom-values.yaml`:

```yaml
# Weather Service configuration
weatherService:
  replicaCount: 3
  
  image:
    repository: your-registry/weather-service
    tag: "1.0.0"
  
  env:
    weatherApiKey: "your-api-key"
    slackBotToken: "xoxb-your-token"
    slackAppId: "A01234567"
  
  ingress:
    enabled: true
    hosts:
      - host: weather-api.yourdomain.com
        paths:
          - path: /
            pathType: Prefix
    tls:
      - secretName: weather-api-tls
        hosts:
          - weather-api.yourdomain.com

# Weather Dashboard configuration
weatherDashboard:
  replicaCount: 2
  
  image:
    repository: your-registry/weather-dashboard
    tag: "1.0.0"
  
  ingress:
    enabled: true
    hosts:
      - host: weather.yourdomain.com
        paths:
          - path: /
            pathType: Prefix
    tls:
      - secretName: weather-dashboard-tls
        hosts:
          - weather.yourdomain.com
```

Install with custom values:

```bash
helm install weather-stack ./helm/weather-stack \
  -f custom-values.yaml \
  --namespace weather
```

### Advanced Configuration

#### Resource Limits

```yaml
weatherService:
  resources:
    limits:
      cpu: 1000m
      memory: 512Mi
    requests:
      cpu: 250m
      memory: 256Mi
```

#### Autoscaling

```yaml
weatherService:
  autoscaling:
    enabled: true
    minReplicas: 3
    maxReplicas: 20
    targetCPUUtilizationPercentage: 70
    targetMemoryUtilizationPercentage: 80
```

#### Node Affinity

```yaml
weatherService:
  nodeSelector:
    node.kubernetes.io/instance-type: t3.large
  
  affinity:
    podAntiAffinity:
      preferredDuringSchedulingIgnoredDuringExecution:
        - weight: 100
          podAffinityTerm:
            labelSelector:
              matchExpressions:
                - key: app.kubernetes.io/name
                  operator: In
                  values:
                    - weather-stack-service
            topologyKey: kubernetes.io/hostname
```

## Production Deployment

### 1. Set Up Ingress Controller

```bash
# Install NGINX Ingress Controller
helm repo add ingress-nginx https://kubernetes.github.io/ingress-nginx
helm install ingress-nginx ingress-nginx/ingress-nginx \
  --namespace ingress-nginx \
  --create-namespace
```

### 2. Install Cert-Manager for TLS

```bash
# Install cert-manager
kubectl apply -f https://github.com/cert-manager/cert-manager/releases/download/v1.13.0/cert-manager.yaml

# Create ClusterIssuer for Let's Encrypt
cat <<EOF | kubectl apply -f -
apiVersion: cert-manager.io/v1
kind: ClusterIssuer
metadata:
  name: letsencrypt-prod
spec:
  acme:
    server: https://acme-v02.api.letsencrypt.org/directory
    email: your-email@example.com
    privateKeySecretRef:
      name: letsencrypt-prod
    solvers:
    - http01:
        ingress:
          class: nginx
EOF
```

### 3. Deploy with Production Values

```bash
helm install weather-stack ./helm/weather-stack \
  -f production-values.yaml \
  --namespace weather \
  --create-namespace
```

### 4. Verify Deployment

```bash
# Check pods
kubectl get pods -n weather

# Check services
kubectl get svc -n weather

# Check ingress
kubectl get ingress -n weather

# View logs
kubectl logs -n weather -l app.kubernetes.io/name=weather-stack-service
kubectl logs -n weather -l app.kubernetes.io/name=weather-stack-dashboard
```

## Monitoring

### Add Prometheus ServiceMonitor

Enable in `values.yaml`:

```yaml
serviceMonitor:
  enabled: true
  interval: 30s
```

### View Metrics

```bash
# Port-forward Prometheus
kubectl port-forward -n monitoring svc/prometheus 9090:9090

# Access at http://localhost:9090
```

## Upgrading

```bash
# Upgrade existing deployment
helm upgrade weather-stack ./helm/weather-stack \
  -f custom-values.yaml \
  --namespace weather

# Rollback if needed
helm rollback weather-stack -n weather
```

## Uninstalling

```bash
# Remove the Helm release
helm uninstall weather-stack -n weather

# Delete secrets
kubectl delete secret weather-secrets -n weather

# Delete namespace
kubectl delete namespace weather
```

## Troubleshooting

### Pods Not Starting

```bash
# Describe pod to see events
kubectl describe pod -n weather <pod-name>

# Check logs
kubectl logs -n weather <pod-name>

# Check resource constraints
kubectl top pods -n weather
```

### Image Pull Errors

```bash
# Create imagePullSecret if using private registry
kubectl create secret docker-registry regcred \
  --docker-server=your-registry.com \
  --docker-username=your-user \
  --docker-password=your-password \
  --docker-email=your-email@example.com \
  -n weather

# Update values.yaml
global:
  imagePullSecrets:
    - regcred
```

### Ingress Not Working

```bash
# Check ingress controller
kubectl get pods -n ingress-nginx

# Verify ingress resource
kubectl describe ingress -n weather

# Check certificate
kubectl describe certificate -n weather
```

### Service Communication Issues

```bash
# Test service connectivity
kubectl run -it --rm debug --image=curlimages/curl --restart=Never -n weather -- sh
# Inside pod:
curl http://weather-stack-service:8080/health
```

## Environment-Specific Configurations

### Development

```yaml
weatherService:
  replicaCount: 1
  resources:
    limits:
      cpu: 200m
      memory: 128Mi
  autoscaling:
    enabled: false
  ingress:
    enabled: false
```

### Staging

```yaml
weatherService:
  replicaCount: 2
  resources:
    limits:
      cpu: 500m
      memory: 256Mi
  autoscaling:
    enabled: true
    minReplicas: 2
    maxReplicas: 5
```

### Production

```yaml
weatherService:
  replicaCount: 3
  resources:
    limits:
      cpu: 1000m
      memory: 512Mi
  autoscaling:
    enabled: true
    minReplicas: 3
    maxReplicas: 20
  podDisruptionBudget:
    enabled: true
    minAvailable: 2
```

## Security Best Practices

1. **Use secrets management** (Sealed Secrets, External Secrets Operator, or Vault)
2. **Enable network policies**
3. **Run as non-root** (already configured in Dockerfiles)
4. **Use resource limits**
5. **Enable pod security policies/standards**
6. **Regularly update images**
7. **Use TLS for all external traffic**

## CI/CD Integration

### Example GitHub Actions Workflow

```yaml
name: Deploy to K8s

on:
  push:
    branches: [main]

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Build and push images
        run: |
          docker build -t ${{ secrets.REGISTRY }}/weather-service:${{ github.sha }} weather/
          docker push ${{ secrets.REGISTRY }}/weather-service:${{ github.sha }}
      
      - name: Deploy with Helm
        run: |
          helm upgrade --install weather-stack ./helm/weather-stack \
            --set weatherService.image.tag=${{ github.sha }} \
            --namespace weather
```

## Support

For issues and questions:
- Check logs: `kubectl logs -n weather -l app.kubernetes.io/component=api`
- Review events: `kubectl get events -n weather --sort-by='.lastTimestamp'`
- Consult main README.md for application-specific details
