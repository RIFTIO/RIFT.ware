
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file yangenums.vala
 * @author Vinod Kamalaraj (vinod.kamalaraj@riftio.com)
 * @date 06/17/2014
 * @brief Vala interface for YangModel enumerations
 * 
 * @abstract
 * This vala specification defines the enumerations that are used by
 * the yang model and cli functions and associated vala interfaces
 */


namespace YangEnums {
  /**
   * Enumeration for Statement Type
   */
  [ CCode ( cprefix = "RW_YANG_STMT_TYPE_", cname = "rw_yang_stmt_type_t" ) ]
  public enum StmtType {
    NULL,
    FIRST,
    ROOT,
    NA,
    ANYXML,
    CONTAINER,
    LEAF,
    LEAF_LIST,
    LIST,
    CHOICE,
    CASE,
    RPC,
    RPCIO,
    NOTIF,
    GROUPING,
    LAST,
    END,  
  }

  /**
   * Yang leaf (or leaf-list entry) type.
   */
  [ CCode ( cprefix = "RW_YANG_LEAF_TYPE_", cname = "rw_yang_leaf_type_t") ]
  public enum LeafType {
    NULL,
    FIRST,
    INT8,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    DECIMAL64,
    EMPTY,
    BOOLEAN,
    ENUM,
    STRING,
    LEAFREF,
    IDREF,
    INSTANCE_ID,
    BITS,
    BINARY,

    ANYXML,  /// Not a leaf type in strict-yang sense, but treated as such by yangmodel
    UNION,   /// Will not be seen in YangValue, but may be seen in YangType
    LAST

  }

  /**
   * Yang Model log level.
   */
  [ CCode ( cprefix = "RW_YANG_LOG_LEVEL_", cname = "rw_yang_log_level_t") ]
  public enum LogLevel {
    NONE,   // Disable all logging
    ERROR,  // Only log errors
    WARN,   // Log errors and warnings
    INFO,   // Log verbose user information
    DEBUG,  // Enable some debugging output
    DEBUG2, // Enable more debugging output
    DEBUG3, // Enable even more debugging output
    DEBUG4  // Enable all debugging output
  }

}

