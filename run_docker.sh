#!/bin/bash
set -e

NUM_NODES=16
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -n|--nodes) NUM_NODES="$2"; shift ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

echo "Generating docker-compose.yml for $NUM_NODES nodes..."
python3 gen_compose.py $NUM_NODES

echo "Starting LazyLog UDP Standalone Cluster ($NUM_NODES nodes)..."
echo "Building the Docker image (this may take 5-10 minutes due to eRPC compilation)..."
docker compose up -d --build

echo "Waiting for all nodes to initialize SSH networking..."
sleep 10

echo "Running figure 6 (Append Latency) benchmark..."
# We execute the script inside node0, which acts as the orchestrator.
# It will SSH into the other container nodes (node0..node15) to start the servers and clients.
docker exec -it lazylog-node0 bash -c "cd /opt/LazyLog-Artifact/scripts && ./fig6.sh && python3 analyze.py"

echo "Benchmark complete! Shutting down cluster..."
docker compose down
