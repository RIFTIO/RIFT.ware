#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#




def main():

  import rw_peas

  # Load rwtrace plugin
  rwtrace = rw_peas.PeasPlugin("rwtrace_plugin-c", 'Rwtrace-1.0')
  engine, info, plugin = rwtrace()    # This is just a shortcut call for convenience
  Rwtrace = rwtrace.typelib

  # Instantiate plugin's API for RWTASKLET component
  rwtrace_module   = plugin.module_init(rwtrace.plugin_name, 'whatever')
  rwtrace_instance = plugin.module_instance_alloc(rwtrace_module, rwtrace.plugin_name + '_instance', 'whatever')

  rwtrace.plugin.set_category_destination(rwtrace_instance, 
                                          Rwtrace.Category.RWTASKLET, 
                                          Rwtrace.Destination.CONSOLE)
  rwtrace.plugin.set_category_severity(rwtrace_instance, 
                                       Rwtrace.Category.RWTASKLET, 
                                       Rwtrace.Severity.ERROR)

  # The following message shows up on the console
  rwtrace.plugin.trace(rwtrace_instance, 
                       Rwtrace.Category.RWTASKLET,
                       Rwtrace.Severity.CRIT, 
                       "Error message")

  # Now lets a try a trace with format string
  message = "test"
  code = -1
  rwtrace.plugin.trace(rwtrace_instance, 
                       Rwtrace.Category.RWTASKLET,
                       Rwtrace.Severity.CRIT, 
                       "Error message: %s[%d]" % (message, code))


  # The following message does NOT show up on the console
  rwtrace.plugin.trace(rwtrace_instance, 
                       Rwtrace.Category.RWTASKLET,
                       Rwtrace.Severity.DEBUG, 
                       "Debug message")

  # Instantiate plugin's API for RATEST component
  rwtrace.plugin.set_category_destination(rwtrace_instance, 
                                          Rwtrace.Category.RATEST, 
                                          Rwtrace.Destination.CONSOLE)
  rwtrace.plugin.set_category_severity(rwtrace_instance, 
                                       Rwtrace.Category.RATEST, 
                                       Rwtrace.Severity.DEBUG)

  # The following message for RATEST appears on the console
  rwtrace.plugin.trace(rwtrace_instance, 
                       Rwtrace.Category.RATEST,
                       Rwtrace.Severity.DEBUG, 
                       "Debug message on console for RATEST")

  rwtrace.plugin.set_category_destination(rwtrace_instance, 
                                          Rwtrace.Category.RATEST, 
                                          Rwtrace.Destination.FILE)

  # The following message for RATEST appears in syslog
  rwtrace.plugin.trace(rwtrace_instance, 
                       Rwtrace.Category.RATEST,
                       Rwtrace.Severity.NOTICE, 
                       "Debug message in the file for RATEST")

if __name__ == "__main__":
    main()
