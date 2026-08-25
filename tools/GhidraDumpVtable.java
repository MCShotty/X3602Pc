// @category XeO3

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public final class GhidraDumpVtable extends GhidraScript {
    @Override
    protected void run() throws Exception {
        if (currentProgram == null) {
            printerr("No current program");
            return;
        }

        String[] arguments = getScriptArgs();
        if (arguments.length == 0) {
            printerr("Expected a case-sensitive symbol-name substring");
            return;
        }
        int entryCount = arguments.length > 1 ? Integer.parseInt(arguments[1]) : 16;

        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(true);
        if (!decompiler.openProgram(currentProgram)) {
            printerr("Decompiler initialization failed: " + decompiler.getLastMessage());
            return;
        }

        try {
            SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
            int matchCount = 0;
            while (symbols.hasNext()) {
                monitor.checkCancelled();
                Symbol symbol = symbols.next();
                if (!symbol.getName(true).contains(arguments[0])) {
                    continue;
                }

                matchCount++;
                Address table = symbol.getAddress();
                println("SYMBOL " + symbol.getName(true) + " " + table);
                for (int index = 0; index < entryCount; ++index) {
                    Address slot = table.add((long) index * 8);
                    long pointer = currentProgram.getMemory().getLong(slot);
                    Address target =
                        currentProgram.getAddressFactory().getDefaultAddressSpace()
                            .getAddress(pointer);
                    Function function =
                        currentProgram.getFunctionManager().getFunctionAt(target);
                    println(
                        "ENTRY " + index + " " + slot + " " + target + " " +
                        (function == null ? "<no-function>" : function.getName()));
                    if (function == null) {
                        continue;
                    }

                    DecompileResults result =
                        decompiler.decompileFunction(function, 180, monitor);
                    if (!result.decompileCompleted()) {
                        printerr(
                            "DECOMPILE_FAILED " + target + " " +
                            result.getErrorMessage());
                        continue;
                    }
                    println("DECOMPILE_BEGIN " + index + " " + target);
                    println(result.getDecompiledFunction().getC());
                    println("DECOMPILE_END " + index + " " + target);
                }
            }
            println("MATCH_COUNT " + matchCount);
        } finally {
            decompiler.dispose();
        }
    }
}
