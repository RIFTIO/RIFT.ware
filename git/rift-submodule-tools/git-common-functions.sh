
# STANDARD_RIFT_IO_COPYRIGHT

# check that you are a top level
if [ ! -f .gitmodules ] ; then
    echo "$(basename $0) can only be executed from top (supermodule) level"
    exit 1
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPT_NAME=$(basename $0)
function log(){
  echo "$SCRIPT_NAME: $@ (in $(pwd))"
}

function check_for_detached_head(){
  git symbolic-ref -q HEAD > /dev/null
  rc=$?
  if [ $rc -ne 0 ]; then
    log "ERROR - Detached head in superproject"
    return 1
  fi

  while read line; do
    pushd $line >/dev/null
      git symbolic-ref -q HEAD > /dev/null
      rc=$?
      if [ $rc -ne 0 ]; then
        log "ERROR - Detached head in submodule: $line"
        return 1
      fi
    popd > /dev/null
  done < <(foreach_initialized_submodule)
}

function check_submodules_clean(){
  submodules="$@"

  while read line; do
    # If submodules variable is not empty and submodule not in the list then
    # skip it.
    if [[ "$submodules" ]] && [[ "$*" != *$line* ]]; then
      continue
    fi

    pushd "$line" >/dev/null
      check_clean
      if [ $? -ne 0 ]; then
        log "ERROR: Unclean submodule: $line"
        return 1
      fi

      $DIR/git-check-unpushed
      if [ $? -ne 0 ]; then
        log "ERROR: Unpushed changes in branch: $line"
        return 1
      fi
    popd > /dev/null

  done < <(foreach_initialized_submodule)
}

function check_clean(){
  ## check for modified content and uncommitted changes
  $DIR/git-check-clean --ignore-submodules=untracked --unstaged --uncommitted --unmerged || return 1
  ## check for untracked files
  if ! ($DIR/git-check-clean --untracked --unstaged --exit-code) ; then
      cat >&2 <<EOF
   Error: Untracked files. Add them to .gitignore
    in the respective submodules or remove them.
    Use "git status" to see where they are.
EOF
      return 1
  fi
}

# A function to replace git submodule foreach using bash.
# Only returns the initialized submodules (not starting with -).
function foreach_initialized_submodule(){
  git submodule | while read line; do
    if [[ $line == -* ]]; then
      continue
    else
      echo $line | cut -d ' ' -f2
    fi
  done
}

function foreach_uninitialized_submodule(){
   git submodule | while read line; do
      if [[ $line == -* ]]; then
        echo $line | cut -d ' ' -f2
      else
        continue
      fi
   done
}

# Foreach all submodules (initalized or uninitialized)
function foreach_submodule(){
   git submodule | while read line; do
        echo $line | cut -d ' ' -f2
   done
}

function is_valid_branch_name(){
  local branch_name="$1"
  git show-ref "$branch_name" > /dev/null 2>&1
  if [ $? -ne 0 ]; then
    log "DEBUG: Branch not found: $branch_name"
    return 1
  fi

  git rev-parse $branch_name > /dev/null 2>&1
  if [ $? -ne 0 ]; then
    log "DEBUG: Branch not found: $branch_name"
    return 1
  fi
}

# Caller should capture stdout of this function get the list of uninitialized submodules.
function get_uninitialized_submodules(){
  while read line; do
    echo -n "$line "
  done < <(foreach_uninitialized_submodule)
}

# Caller should capture stdout of this function get the list of initialized submodules.
function get_initialized_submodules(){
  while read line; do
    echo -n "$line "
  done < <(foreach_initialized_submodule)
}

# Caller should capture stdout of this function get the entire list of submodules.
function get_all_submodules(){
  while read line; do
    echo -n "$line "
  done < <(foreach_submodule)
}

# Get the current branch name
# Returns the branch name if on a branch, or HEAD otherwise.
function get_current_branch(){
  echo "$(git rev-parse --abbrev-ref HEAD)"
}

function ensure_on_local(){
  if [[ "$(get_current_branch)" != "local" ]]; then
    log "(ERROR) Not on local branch. Cannot continue."
    return 1
  fi
}

