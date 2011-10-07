mv ~/.bashrc ~/.bashrc_orig
echo "PS1=''" >> ~/.bashrc
echo "echo 'foo'" >> ~/.bashrc
echo "#comment" >> ~/.bashrc
echo "ls test2.txt" >> ~/.bashrc
echo "echo 'foobar'" >> ~/.bashrc
cp ~/.bashrc ~/.tshrc
