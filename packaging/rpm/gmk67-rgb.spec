Name:           gmk67-rgb
Version:        1.0.0
Release:        1
Summary:        RGB lighting control for the Zuoya GMK67 keyboard
License:        GPL-3.0-or-later
URL:            https://github.com/aloran-dev/gmk67-rgb
Source0:        %{name}-%{version}.tar.gz
BuildArch:      noarch

BuildRequires:  make
BuildRequires:  systemd-rpm-macros
Requires:       python3 >= 3.10
Requires:       python3-gobject
Requires:       gtk4 >= 4.12
Requires:       libadwaita >= 1.4

%description
GTK4/libadwaita app to set the lighting effect, color, brightness, speed and
direction of the Zuoya GMK67 (v1, 05ac:024f) keyboard over USB. Installs a
udev rule that gives the logged-in user access to the keyboard's hidraw device.

%prep
%autosetup

%build

%install
%make_install PREFIX=%{_prefix} UDEVDIR=%{_udevrulesdir}

%post
%udev_rules_update
udevadm trigger --subsystem-match=hidraw >/dev/null 2>&1 || :

%postun
%udev_rules_update

%files
%license LICENSE
%doc README.md
%{_bindir}/gmk67-rgb
%{_bindir}/gmk67-fx
%{_datadir}/gmk67-rgb/
%{_datadir}/applications/io.github.aloran_dev.Gmk67Rgb.desktop
%{_datadir}/metainfo/io.github.aloran_dev.Gmk67Rgb.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/io.github.aloran_dev.Gmk67Rgb.svg
%{_udevrulesdir}/70-gmk67.rules

%changelog
* Sat Oct 24 2026 Daniel Alvarez <criticalgeek1@gmail.com> - 1.0.0-1
- First release
