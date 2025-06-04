scp -r -O 03.fs/02.appfs/www root@192.168.2.100:/tmp/
sshpass -p '##u3_rec' scp -r -O lighttpd.sh root@192.168.2.100:/tmp/
sshpass -p '##u3_rec' scp -r -O lighttpd.conf root@192.168.2.100:/tmp/
echo "-------------------------"
echo "chmod +x /tmp/lighttpd.sh && /tmp/lighttpd.sh stop"

sshpass -p '##u3_rec' ssh root@192.168.2.100
