AGENT_MODE_ENV_FILE="/var/rift/env.d/RWMGMT_AGENT_MODE"
if [ -e ${AGENT_MODE_ENV_FILE} ]
then
  read -r mode < ${AGENT_MODE_ENV_FILE}
  if [[ "$mode" == "CONFD" ]]
  then
    echo "------------- show uagent confd --------------------------"
    show uagent confd
  fi
fi
echo "------------- show uagent state --------------------------"
show uagent state
echo "------------- show uagent last-error ---------------------"
show uagent last-error
echo "------------- show-agent-logs ----------------------------"
show-agent-logs string
echo "----------------------------------------------------------"

