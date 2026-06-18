{
  description = "Qt6 Cava Monitor QML Plugin";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        cavamonitor = pkgs.stdenv.mkDerivation {
          pname = "qt6-cava-plugin";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.pkg-config
            pkgs.qt6.wrapQtAppsHook
          ];
          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtdeclarative
            pkgs.pipewire
            pkgs.fftw
          ];
          cmakeFlags = [
            "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}/lib/qt6/qml"
          ];
        };
      in {
        packages = {
          inherit cavamonitor;
          default = cavamonitor;
        };
        devShells.default = pkgs.mkShell {
          name = "cavamonitor-dev";
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.pkg-config
            pkgs.qt6.wrapQtAppsHook
          ];
          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtdeclarative
            pkgs.pipewire
            pkgs.fftw
          ];
        };
      }
    );
}
