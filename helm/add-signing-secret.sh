#!/bin/bash
# Quick script to add SLACK_SIGNING_SECRET to existing Kubernetes deployment
# Usage: SLACK_SIGNING_SECRET=your_secret ./add-signing-secret.sh

set -e

NAMESPACE="${NAMESPACE:-weather}"
SECRET_NAME="${SECRET_NAME:-weather-secrets}"

if [ -z "$SLACK_SIGNING_SECRET" ]; then
    echo "Error: SLACK_SIGNING_SECRET environment variable not set"
    echo ""
    echo "Usage:"
    echo "  SLACK_SIGNING_SECRET=your_secret ./add-signing-secret.sh"
    echo ""
    echo "Or with custom namespace:"
    echo "  NAMESPACE=my-namespace SLACK_SIGNING_SECRET=your_secret ./add-signing-secret.sh"
    exit 1
fi

echo "Adding SLACK_SIGNING_SECRET to secret '$SECRET_NAME' in namespace '$NAMESPACE'..."

# Check if secret exists
if ! kubectl get secret $SECRET_NAME -n $NAMESPACE &> /dev/null; then
    echo "Error: Secret '$SECRET_NAME' not found in namespace '$NAMESPACE'"
    echo "Create it first with:"
    echo "  kubectl create secret generic $SECRET_NAME \\"
    echo "    --from-literal=WEATHERAPI_KEY=your_key \\"
    echo "    --from-literal=SLACK_BOT_TOKEN=xoxb-your-token \\"
    echo "    --from-literal=SLACK_SIGNING_SECRET=your_secret \\"
    echo "    --from-literal=SLACK_APP_ID=A01234567 \\"
    echo "    -n $NAMESPACE"
    exit 1
fi

# Check if SLACK_SIGNING_SECRET already exists
if kubectl get secret $SECRET_NAME -n $NAMESPACE -o jsonpath='{.data.SLACK_SIGNING_SECRET}' 2>/dev/null | grep -q .; then
    echo "SLACK_SIGNING_SECRET already exists. Updating..."
    OP="replace"
else
    echo "SLACK_SIGNING_SECRET not found. Adding..."
    OP="add"
fi

# Patch the secret
kubectl patch secret $SECRET_NAME -n $NAMESPACE \
  --type='json' \
  -p='[{"op": "'$OP'", "path": "/data/SLACK_SIGNING_SECRET", "value": "'$(echo -n "$SLACK_SIGNING_SECRET" | base64)'"}]'

echo "✓ Secret updated successfully"

# Restart the deployment to pick up the new secret
if kubectl get deployment weather-stack-service -n $NAMESPACE &> /dev/null; then
    echo ""
    echo "Restarting weather-stack-service deployment..."
    kubectl rollout restart deployment -n $NAMESPACE weather-stack-service
    echo "✓ Deployment restarted"
    echo ""
    echo "Monitor the rollout with:"
    echo "  kubectl rollout status deployment -n $NAMESPACE weather-stack-service"
else
    echo ""
    echo "⚠ Deployment 'weather-stack-service' not found in namespace '$NAMESPACE'"
    echo "The secret has been updated, but you'll need to restart your pods manually."
fi
