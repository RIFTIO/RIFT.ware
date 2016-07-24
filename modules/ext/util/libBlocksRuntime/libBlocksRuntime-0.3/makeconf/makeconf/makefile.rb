# A Makefile is a collection of targets and rules used to build software.
#
class Makefile
  
  # Object constructor.
  def initialize
    @vars = {}
    @targets = {}

    %w[all check clean distclean install uninstall distdir].each do |x|
        add_target(x)
    end
  end

  # Define a variable within the Makefile
  def define_variable(lval,op,rval)
    throw "invalid arguments" if lval.nil? or op.nil? 
    throw "variable `#{lval}' is undefined" if rval.nil?
    @vars[lval] = [ op, rval ]
  end

  def target(object)
    @targets[object]
  end

  def merge!(src)
    return if src.nil?
    throw 'invalid argument' unless src.is_a?(Makefile)
    @vars.merge!(src.vars)
    src.targets.each do |k,v|
      if targets.has_key?(k)
         targets[k].merge!(v)
      else
         targets[k] = (v)
      end
    end
  end

  def add_target(object,depends = [], rules = [])
    if object.kind_of?(Target)
      @targets[object.objs] = object
    else
      @targets[object] = Target.new(object,depends,rules)
    end
  end

  def add_rule(target, rule)
    add_target(target, [], []) unless @targets.has_key? target
    @targets[target].add_rule(rule)
  end

  # Add a file to the tarball during 'make dist'
  def distribute(path)
    path = [ path ] if path.kind_of? String

    path.each do |src|
      # FIXME: support Windows backslashery
      if src =~ /\//
         dst = '$(distdir)/' + File.dirname(src)
         @targets['distdir'].mkdir(dst)
         @targets['distdir'].cp(src, dst)
      else
         @targets['distdir'].cp(src, '$(distdir)')
      end
    end
  end

  # Add a file to be removed during 'make clean'
  # TODO: optimize by eliminating multiple rm(1) fork/exec
  def clean(path)
    add_rule('clean', Platform.rm(Platform.pathspec(path)))
  end

  # Add a file to be removed during 'make distclean'
  # TODO: optimize by eliminating multiple rm(1) fork/exec
  def distclean(path)
    add_rule('distclean', Platform.rm(Platform.pathspec(path)))
  end

  def add_dependency(target,depends)
    add_target(target, [depends], []) unless @targets.has_key? target
    @targets[target].add_dependency(depends)
  end

  def make_dist(project,version)
    distdir = project + '-' + version.to_s
    # XXX-FIXME this should come from the Project
    distfile = project + '-' + version.to_s + '.tar.gz'
    tg = Target.new(distfile)
    tg.add_rule(Platform.rmdir(distdir))
    tg.add_rule("mkdir " + distdir)
    tg.add_rule('$(MAKE) distdir distdir=' + distdir)
    if Platform.is_windows? 

# FIXME - Not implemented yet

    else
       tg.add_rule("rm -rf #{distdir}.tar #{distdir}.tar.gz")
       tg.add_rule("tar cf #{distdir}.tar #{distdir}")
       tg.add_rule("gzip #{distdir}.tar")
       tg.add_rule("rm -rf #{distdir}")
       clean("#{distdir}.tar.gz")
    end
    @targets[distfile] = tg
  end

  def to_s
    res = ''
    @vars.sort.each { |x,y| res += [x, y[0], y[1]].join(' ') + "\n" }
    res += "\n\n"
    res += "default: all\n"
    targets.each { |x,y| throw "#{x} is broken" unless y.is_a? Target }
    @targets.sort.each { |x,y| res += y.to_s }
    res
  end

  def write(path)
    f = File.open(path, 'w')
    f.print "# AUTOMATICALLY GENERATED -- DO NOT EDIT\n"
    f.print self.to_s
    f.close
  end

  protected

  attr_reader :vars, :targets
end
