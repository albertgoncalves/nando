with import <nixpkgs> {};
mkShell.override { stdenv = llvmPackages_14.stdenv; } {
    buildInputs = [
        linuxPackages.perf
        mold
        openjdk
        shellcheck
        unzip
        wget
    ];
    shellHook = ''
        . .shellhook
    '';
    hardeningDisable = [ "all" ];
}
