#!/usr/bin/env python3

import argparse
import itertools
import sys

YANG_TEMPLATE = """
/** AUTO GENERATED FILE -- DO NOT EDIT */

/**
 * @file {category_name}-log.yang
 * @brief RiftWare Log Event Definitions
 */


module {category_name}
{{
  namespace "http://riftio.com/ns/riftware-1.0/{category_name}";
  prefix "{category_name}";

  import rw-base {{
    prefix rwbase;
  }}

  import rw-pb-ext {{
    prefix "rwpb";
  }}

  import rw-yang-types {{
    prefix "rwt";
  }}

  import rw-notify-ext {{
    prefix "rwnotify";
  }}

  import rw-log {{
    prefix "rwlog";
  }}

  /*
   * Generic Logger Log Events
   */
  notification debug {{
    rwpb:msg-new Debug;
    rwnotify:log-event-id {debug_id};
      description
         "Generic Debug Log";
      uses rwlog:severity-debug;
      leaf category {{
        type string;
      }}
      leaf logger {{
        type string;
      }}
      leaf log  {{
        type string;
      }}
  }}

  notification info {{
    rwpb:msg-new Info;
    rwnotify:log-event-id {info_id};
      description
         "Generic Info Log";
      uses rwlog:severity-info;
      leaf category {{
        type string;
      }}
      leaf logger {{
        type string;
      }}
      leaf log  {{
        type string;
      }}
  }}

  notification warn {{
    rwpb:msg-new Warn;
    rwnotify:log-event-id {warn_id};
      description
         "Generic Warning Log";
      uses rwlog:severity-warning;
      leaf category {{
        type string;
      }}
      leaf logger {{
        type string;
      }}
      leaf log  {{
        type string;
      }}
  }}

  notification error {{
    rwpb:msg-new Error;
    rwnotify:log-event-id {error_id};
      description
         "Generic Error Log";
      uses rwlog:severity-error;
      leaf category {{
        type string;
      }}
      leaf logger {{
        type string;
      }}
      leaf log  {{
        type string;
      }}
  }}

  notification critical {{
    rwpb:msg-new Critical;
    rwnotify:log-event-id {critical_id};
      description
         "Generic Critical Log";
      uses rwlog:severity-critical;
      leaf category {{
        type string;
      }}
      leaf logger {{
        type string;
      }}
      leaf log  {{
        type string;
      }}
  }}

  /*
   * END - generic log events
   */
}}
"""

def main(argv=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-file", required=True, type=argparse.FileType('w'))
    parser.add_argument("--category-name", required=True)
    parser.add_argument("--start-event-id", required=True, type=int)
    args = parser.parse_args(argv)

    event_id_iter = itertools.count(args.start_event_id)
    yang_str = YANG_TEMPLATE.format(
            category_name=args.category_name,
            debug_id=next(event_id_iter),
            info_id=next(event_id_iter),
            warn_id=next(event_id_iter),
            error_id=next(event_id_iter),
            critical_id=next(event_id_iter),
            )

    args.output_file.write(yang_str)
    args.output_file.flush()
    args.output_file.close()

if __name__ == "__main__":
    main()
