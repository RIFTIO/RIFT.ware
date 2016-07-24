# A project contains all of the information about the build.
#
class Project
 
  require 'makeconf/systemtype'
  require 'makeconf/baseproject'

  def self.new(options)
    if SystemType.host =~ /-androideabi$/
       require 'makeconf/androidproject'
       object = AndroidProject.allocate
    else
       object = BaseProject.allocate
    end
    object.send :initialize, options
    object
  end

end
