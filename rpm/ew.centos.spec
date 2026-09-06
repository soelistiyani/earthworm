%define name    earthworm
%define version 7.10
%define release 2
%define ewhome "/opt/earthworm"
%define version_dir %{ew_home}/%{name}_%{version}
Summary: Open source seismic data acquisition and automatic earthquake processing software suite
License: Free
Name: earthworm
Vendor: ISTI
Packager: Alexander Schnackenberg (a.schnackenberg@isti.com)
#Provides: earthworm = %{version}-%{release}
Version: %{version}
Release: %{release}
Source: %{name}_%{version}.tar.gz
URL: http://earthworm.isti.com
BuildRoot: /tmp/earthworm/
BuildRequires: gcc-c++
BuildRequires: gcc-gfortran
Requires: libgfortran

%description
Earthworm provides users with an advanced open source seismic data acquisition and processing software
package capable of calculating earthquake locations and magnitudes.

The Earthworm system is a robust a mature software tool, incorporating over 25 years of design and
development. 

%prep
# set name of build directory to /opt/earthworm, then create and cd to that directory
# %setup -c -n %{ew_home}
%setup -n %{name}_%{version} 

%build
source environment/rpm_env.bash
cd src
make clean_unix
make unix

%install
#broken binaries included with Mole:
rm -rf $RPM_BUILD_DIR/%{name}_%{version}/src/archiving/mole/
rm -rf $RPM_BUILD_DIR/%{name}_%{version}/src/data_sources/gcf2ew/
rm -rf $RPM_BUILD_DIR/%{name}_%{version}/src/data_exchange/sendfile_srv
install -d $RPM_BUILD_ROOT/opt/earthworm/%{name}_%{version}
cp -r $RPM_BUILD_DIR/%{name}_%{version}/ $RPM_BUILD_ROOT/opt/earthworm/

%clean
#rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%config /opt/earthworm/%{name}_%{version}/params
%config /opt/earthworm/%{name}_%{version}/environment
%doc /opt/earthworm/%{name}_%{version}/release_notes*
%doc /opt/earthworm/%{name}_%{version}/README*
%docdir /opt/earthworm/%{name}_%{version}/ewdoc
/opt/earthworm/%{name}_%{version}/*

