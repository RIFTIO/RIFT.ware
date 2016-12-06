RIFT_NB=${RIFT_INSTALL}/var/rift/schema/version/latest/northbound/yang/
echo "------------- current Time ------------------------"
date
echo "------------- show config -------------------------"
show config


if [ ${RIFT_PLATFORM} = "ub16" ]
then
    echo "------------- apt list | grep rift ----------------"
    apt list --installed | grep rift
    echo "---------------------------------------------------"
else
    echo "------------- rpm -qa | grep rift -----------------"
    rpm -qa | grep rift
    echo "---------------------------------------------------"
fi

for file in `ls ${RIFT_VAR_ROOT}/schema/cli/*.cli.ssi.sh`
do
  afile=$file

  # Check if the related yang file exists in the current northbound
  # If not skip executing the ssi
  ybase=$(basename $file)
  ybase=${ybase%%.*}
  if [[ ! -f ${RIFT_NB}${ybase}.yang ]]
  then
    continue
  fi

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
