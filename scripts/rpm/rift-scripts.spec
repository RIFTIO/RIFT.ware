Name:           rift_scripts
Version:        %{?RELEASE}
Release:        %{?BUILD}%{?dist}
Summary:        rift scripts

License:        APL
URL:            http://www.riftio.com
AutoReq: no
BuildArch:      noarch


BuildRequires:  python
Requires:       python

%description
rift scripts


%prep


%build


%install
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
tar xf $RPM_SOURCE_DIR/rift-scripts.tar.xz -C $RPM_BUILD_ROOT


%pre

# remove /usr/rift symlink if it exists which comes from internal labmode
RDIR="/usr/rift"

if [ -d "$RDIR" ]; then
  if [ -L "$RDIR" ]; then
    # it is a symlink
    rm "$RDIR"
  fi
fi
# remove /usr/rift symlink if it exists which comes from internal labmode



%files
%doc
/usr/rift



%changelog
* Tue Jan 13 2015 Jeremy Mordkoff
- 
