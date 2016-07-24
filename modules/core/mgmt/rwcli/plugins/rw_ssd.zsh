echo "------------- current Time ------------------------"
date
echo "------------- show config -------------------------"
show config
echo "------------- rpm -qa | grep rift -----------------"
rpm -qa | grep rift
echo "---------------------------------------------------"
for file in `ls ${RIFT_INSTALL}/var/rift/schema/cli/*.cli.ssi.sh`
do
  afile=$file
  if [[ -h $file ]]
  then
     afile=`realpath $file`
  fi
  . $afile
done

#invoke sysdump on all VMs.
vm_ips=($(show vcs info | grep -o '[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}' | sort | uniq))
local_prefix="127.0."

for ip in ${vm_ips}
do
  if [[ $ip != $prefix* ]]
  then
    echo "-------------------------- rwsysdump -------------------------------------"
    /usr/rift/bin/ssh_root ${ip} -o ConnectTimeout=10 ${RIFT_ROOT}/rift-shell -e -- rwsysdump --stdout
    echo "--------------------------------------------------------------------------"
  fi
done

#Do not worry about duplicate outputs, invoke for the local VM
echo "-------------------rwsysdump-----------------------------------"
rwsysdump --stdout
echo "---------------------------------------------------------------"
