# -*- mode: ruby -*-
# vi: set ft=ruby :

$setup = <<SCRIPT
  make=$(which make)
  [ $? -eq 1 ] && apt-get install make -y
  echo "export WFU_WEBMS_DIR=/waifu.xyz/webms" > /etc/profile.d/waifu.sh
  echo "export WFU_DB_LOCATION=/waifu.xyz/webms/waifu.db" >> /etc/profile.d/waifu.sh
  cd /waifu.xyz && make clean && make
SCRIPT

Vagrant.configure("2") do |config|
  config.vm.box      = "precise64"
  config.vm.box_url  = "http://files.vagrantup.com/precise64.box"
  config.vm.hostname = "waifu.xyz"
  config.vm.synced_folder ".", "/waifu.xyz"
  config.vm.provision :shell, inline: $setup
end
