#!/bin/bash

INSTALL_DIR=$HOME/touchndn

echo "*************************************************************************"
echo " TouchNDN Initial Setup script"
echo ""
echo " This script will install tools and libraries required for TouchNDN "
echo " framework. You may be asked for your administrator password."
echo "*************************************************************************"

# setup prerequisites
# install brew, python3 and virtualenv
echo "installing dev tools..."
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
if [ $? -ne 0 ]; then
    echo "brew install failed"
    exit 1
fi

brew install python boost openssl pkg-config
if [ $? -ne 0 ]; then
    echo "brew packages install failed"
    exit 1
fi

pip3 install virtualenv
if [ $? -ne 0 ]; then
    echo "virtualenv install failed"
    exit 1
fi

# setup NDN platform
echo "setting up NDN platform..."
mkdir -p $INSTALL_DIR
cd $INSTALL_DIR
git clone https://github.com/named-data/ndn-cxx.git
cd ndn-cxx
git checkout ndn-cxx-0.6.6
./waf configure
./waf
sudo ./waf install

if [ $? -ne 0 ]; then
    echo "ndn-cxx install failed"
    exit 1
fi

cd $INSTALL_DIR
git clone --recursive https://github.com/named-data/NFD.git
cd NFD
git checkout NFD-0.6.6
./waf configure
./waf
sudo ./waf install

if [ $? -ne 0 ]; then
    echo "NFD install failed"
    exit 1
fi

echo "configuring NFD security..."
sudo cp /usr/local/etc/ndn/nfd.conf.sample /usr/local/etc/ndn/nfd.conf
ndnsec-keygen /`whoami` | ndnsec-install-cert -
sudo mkdir -p /usr/local/etc/ndn/keys
ndnsec-cert-dump -i /`whoami` > default.ndncert
sudo mv default.ndncert /usr/local/etc/ndn/keys/default.ndncert

if [ $? -ne 0 ]; then
    echo "NFD security configuration failed"
    exit 1
fi

# installing PyNDN and PyCNL
echo "installing NDN libraries..."
cd $INSTALL_DIR
pip3 install pyndn
git clone https://github.com/named-data/PyCNL.git
echo "export PYTHONPATH=\$PYTHONPATH:$INSTALL_DIR/PyCNL/python" >> ~/.bash_profile
if [ $? -ne 0 ]; then
    echo "installing python NDN libraries failed"
    exit 1
fi
source ~/.bash_profile

# tapping into remap's brew
echo "installing TouchNDN supporting tools..."
brew tap remap/touchndn
brew install touchndn

if [ $? -ne 0 ]; then
    echo "brew tap failed"
    exit 1
fi

cd $INSTALL_DIR
git clone https://github.com/remap/TouchNDN.git repo
echo "done."
