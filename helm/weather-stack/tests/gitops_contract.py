"""Check secret-free adoption rendering without a cluster or credentials."""
from pathlib import Path
import subprocess
import tempfile

import yaml


chart = Path(__file__).resolve().parents[1]
digest = "sha256:" + "a" * 64
values = {
    "weatherService": {
        "existingSecret": "weather-secrets",
        "image": {"repository": "ghcr.io/hbjoroy/weather-service", "digest": digest},
        "podAnnotations": {"adoption-test": "preserved"},
    },
    "weatherDashboard": {
        "image": {"tag": "1.0.22"},
        "existingDatabaseSecret": "postgres-credentials",
        "existingOidcSecret": "oidc-credentials",
        "podAnnotations": {"adoption-test": "dashboard-preserved"},
    },
}
with tempfile.TemporaryDirectory() as temporary:
    overrides = Path(temporary) / "values.yaml"
    overrides.write_text(yaml.safe_dump(values))
    subprocess.run(["helm", "lint", str(chart), "-f", str(overrides)], check=True)
    rendered = subprocess.check_output(
        ["helm", "template", "weather-stack", str(chart), "-n", "weather", "-f", str(overrides)],
        text=True,
    )
    documents = [document for document in yaml.safe_load_all(rendered) if document]

assert not any(document["kind"] == "Secret" for document in documents)
assert len(documents) == 7
deployments = {
    document["metadata"]["name"]: document
    for document in documents if document["kind"] == "Deployment"
}
for name, deployment in deployments.items():
    assert "replicas" not in deployment["spec"], name
    selector = deployment["spec"]["selector"]["matchLabels"]
    assert selector == deployment["spec"]["template"]["metadata"]["labels"]
    assert selector["app.kubernetes.io/instance"] == "weather-stack"
    assert selector["app.kubernetes.io/name"] == name
service = deployments["weather-stack-service"]["spec"]["template"]
assert service["spec"]["containers"][0]["image"] == "ghcr.io/hbjoroy/weather-service@" + digest
assert service["spec"]["containers"][0]["envFrom"] == [{"secretRef": {"name": "weather-secrets"}}]
assert service["metadata"]["annotations"] == {"adoption-test": "preserved"}
dashboard = deployments["weather-stack-dashboard"]["spec"]["template"]
assert dashboard["spec"]["containers"][0]["image"] == "hbjoroy/weather-dashboard:1.0.22"
assert dashboard["metadata"]["annotations"] == {"adoption-test": "dashboard-preserved"}
references = {
    entry["valueFrom"]["secretKeyRef"]["name"]
    for entry in dashboard["spec"]["containers"][0]["env"] if "valueFrom" in entry
}
assert references == {"postgres-credentials", "oidc-credentials"}
print("Secret-free GitOps rendering, digest, identities and dashboard preservation pass.")
