# -*- mode: ruby -*-
# vi: set ft=ruby :

$setup = <<SCRIPT
  make=$(which make)
  [ $? -eq 1 ] && sudo apt-get install make -y
  echo "export WFU_WEBMS_DIR=/waifu.xyz/webms" >> /home/vagrant/.bashrc
  echo "export WFU_DB_LOCATION=/waifu.xyz/webms/waifu.db" >> /home/vagrant/.bashrc
  cd /waifu.xyz && make clean && make
  echo "waifu.xyz downloaded and built"
SCRIPT

Vagrant.configure("2") do |config|
  config.vm.box      = "precise64"
  config.vm.box_url  = "http://files.vagrantup.com/precise64.box"
  config.vm.hostname = "waifu.xyz"
  config.vm.synced_folder ".", "/waifu.xyz"
  config.vm.provision :shell, inline: $setup, privileged: false
end
