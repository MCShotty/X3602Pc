// Decompile every function containing a reference to a symbol or address.
// @category XeO3

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.LinkedHashSet;
import java.util.Set;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class DecompileReferencing extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length != 2 && arguments.length != 3) {
            throw new IllegalArgumentException(
                "Usage: DecompileReferencing <symbol-or-address> <output-directory> [expected-executable-sha256]");
        }

        String executableHash = currentProgram.getExecutableSHA256();
        if (arguments.length == 3 &&
            !arguments[2].equalsIgnoreCase(executableHash)) {
            throw new IllegalStateException("Executable SHA-256 mismatch: " + executableHash);
        }

        Set<Address> targets = new LinkedHashSet<>();
        Address parsed = null;
        if (arguments[0].matches("(?i)[0-9a-f]+")) {
            parsed = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(arguments[0]);
        }
        if (parsed != null) {
            targets.add(parsed);
        } else {
            SymbolIterator symbols = currentProgram.getSymbolTable()
                .getSymbols(arguments[0]);
            while (symbols.hasNext()) {
                targets.add(symbols.next().getAddress());
            }
        }
        if (targets.isEmpty()) {
            throw new IllegalStateException("No target found for " + arguments[0]);
        }

        Set<Function> functions = new LinkedHashSet<>();
        for (Address target : targets) {
            for (Reference reference : getReferencesTo(target)) {
                Function function = getFunctionContaining(reference.getFromAddress());
                if (function != null) {
                    functions.add(function);
                }
            }
        }

        File outputDirectory = new File(arguments[1]);
        Files.createDirectories(outputDirectory.toPath());
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            for (Function function : functions) {
                DecompileResults results = decompiler.decompileFunction(
                    function, 120, monitor);
                if (!results.decompileCompleted()) {
                    printerr("Unable to decompile " + function.getName() + ": " +
                        results.getErrorMessage());
                    continue;
                }
                String filename = function.getEntryPoint() + ".c";
                String header = String.format(
                    "// %s at %s%n// Executable SHA-256: %s%n%n",
                    function.getName(), function.getEntryPoint(), executableHash);
                Files.writeString(new File(outputDirectory, filename).toPath(),
                    header + results.getDecompiledFunction().getC(),
                    StandardCharsets.UTF_8);
            }
        } finally {
            decompiler.dispose();
        }
        println("Decompiled " + functions.size() + " referencing functions.");
    }
}
