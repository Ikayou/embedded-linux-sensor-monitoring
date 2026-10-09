output "proxmox_nodes" {
  value = data.proxmox_virtual_environment_nodes.nodes.names
}