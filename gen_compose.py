import sys

nodes = 16
if len(sys.argv) > 1:
    nodes = int(sys.argv[1])

template = """version: '3'
services:
"""

for i in range(nodes):
    template += f"""  node{i}:
    build:
      context: .
      dockerfile: Dockerfile
      args:
        - http_proxy=${{http_proxy:-}}
        - https_proxy=${{https_proxy:-}}
        - HTTP_PROXY=${{HTTP_PROXY:-}}
        - HTTPS_PROXY=${{HTTPS_PROXY:-}}
    environment:
      - http_proxy=${{http_proxy:-}}
      - https_proxy=${{https_proxy:-}}
      - HTTP_PROXY=${{HTTP_PROXY:-}}
      - HTTPS_PROXY=${{HTTPS_PROXY:-}}
    container_name: lazylog-node{i}
    hostname: node{i}
    networks:
      lazylog-net:
        ipv4_address: 10.10.2.{10+i}
    cap_add:
      - NET_ADMIN
"""

template += """
networks:
  lazylog-net:
    driver: bridge
    ipam:
      config:
        - subnet: 10.10.2.0/24
"""

with open('docker-compose.yml', 'w') as f:
    f.write(template)
print("Generated docker-compose.standalone.yml")
