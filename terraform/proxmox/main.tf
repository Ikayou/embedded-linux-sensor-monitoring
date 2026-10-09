terraform {
  required_providers {
    proxmox = {
      source = "bpg/proxmox"
    }
  }
}

provider "proxmox" {
  endpoint  = "https://192.168.178.52:8006/"
  api_token = var.proxmox_api_token
  insecure  = true
}

data "proxmox_virtual_environment_nodes" "nodes" {}

resource "proxmox_virtual_environment_vm" "lab_vm01" {
  name      = "lab-vm01"
  node_name = "pve"
  vm_id     = 101

  clone {
    vm_id = 9000
    full  = true
  }

  cpu {
    cores = 2
  }

  memory {
    dedicated = 2048
  }

  disk {
    datastore_id = "local-lvm"
    interface    = "scsi0"
    size         = 20
  }

  network_device {
    bridge = "vmbr0"
  }

  initialization {
    datastore_id = "local-lvm"

    user_account {
      username = "yuichiro"

      keys = [
        var.ssh_public_key
      ]
    }

    ip_config {
      ipv4 {
        address = "10.10.10.20/24"
        gateway = "10.10.10.1"
      }
    }
  }

  agent {
    enabled = false
  }
}

resource "proxmox_virtual_environment_vm" "ansible01" {
  name      = "ansible01"
  node_name = "pve"
  vm_id     = 102

  clone {
    vm_id = 9000
    full  = true
  }

  cpu {
    cores = 1
  }

  memory {
    dedicated = 1024
  }

  disk {
    datastore_id = "local-lvm"
    interface    = "scsi0"
    size         = 20
  }

  network_device {
    bridge = "vmbr0"
  }

  initialization {
    datastore_id = "local-lvm"

    user_account {
      username = "yuichiro"

      keys = [
        var.ssh_public_key
      ]
    }

    ip_config {
      ipv4 {
        address = "10.10.10.30/24"
        gateway = "10.10.10.1"
      }
    }
  }

  agent {
    enabled = false
  }
}