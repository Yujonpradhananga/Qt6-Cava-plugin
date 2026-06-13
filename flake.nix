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

        libcava = pkgs.stdenv.mkDerivation {
          pname = "libcava";
          version = "0.10.7";

          src = pkgs.fetchFromGitHub {
            owner = "karlstav";
            repo = "cava";
            tag = "0.10.7";
            hash = "sha256-eOGUDGGlja5Cq8XTJFRqyP6qyaoxOJm09vZrlk4KS9k=";
          };

          buildInputs = [ pkgs.fftw ];
          nativeBuildInputs = [ pkgs.pkg-config ];

          buildPhase = ''
            gcc -shared -fPIC -o libcava.so cavacore.c \
              -lm -lfftw3 \
              -I. \
              $(pkg-config --cflags --libs fftw3)
          '';

          installPhase = ''
            mkdir -p $out/lib $out/include/cava $out/lib/pkgconfig

            cp libcava.so $out/lib/
            cp cavacore.h $out/include/cava/

            cat > $out/lib/pkgconfig/cava.pc << EOF
            prefix=$out
            libdir=\''${prefix}/lib
            includedir=\''${prefix}/include

            Name: cava
            Description: Cava core audio processing library
            Version: 0.10.7
            Libs: -L\''${libdir} -lcava
            Cflags: -I\''${includedir}
            EOF
          '';
        };

        # The main plugin package
        cavamonitor = pkgs.stdenv.mkDerivation {
          pname = "qt6-cava-plugin";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.pkg-config
            pkgs.qt6.wrapQtAppsHook
            pkgs.patchelf
          ];

          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtdeclarative
            pkgs.pipewire
            pkgs.fftw
            libcava
          ];

          cmakeFlags = [
            "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}/lib/qt6/qml"
          ];

          postFixup = ''
            patchelf --add-rpath '$ORIGIN' \
              $out/lib/qt6/qml/CavaMonitor/libcavamonitorplugin.so
          '';
        };

      in {
        packages = {
          inherit libcava cavamonitor;
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
            libcava
          ];

          shellHook = ''
            echo "🎵 CavaMonitor dev shell"
            echo "  libcava:    ${libcava}"
            echo ""
            echo "Build with:"
            echo "  mkdir -p build && cd build"
            echo "  cmake .. -DCMAKE_INSTALL_PREFIX=\$HOME/.local/lib/qt6/qml"
            echo "  make -j\$(nproc)"
            echo "  make install"
            echo ""
            echo "Or build via flake:"
            echo "  nix build"
          '';
        };
      }
    );
}
