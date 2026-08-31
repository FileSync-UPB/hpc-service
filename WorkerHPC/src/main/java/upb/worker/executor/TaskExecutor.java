package upb.worker.executor;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;

import java.io.File;
import java.io.FileWriter;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class TaskExecutor {

    private final ObjectMapper objectMapper = new ObjectMapper();

    /**
     * Motor de ejecución de código real en Python.
     * Crea un directorio temporal aislado, escribe el script y el chunk de datos en JSON,
     * invoca el ejecutor de Python ('python' o 'python3') y lee la salida JSON generada.
     */
    public List<Object> execute(String pythonCode, List<Object> datasetChunk) {
        List<Object> results = new ArrayList<>();
        if (datasetChunk == null) {
            return results;
        }

        // Si el código no parece Python o está vacío, fallback
        if (pythonCode == null || pythonCode.isBlank()) {
            return datasetChunk;
        }

        Path tempDir = null;
        try {
            // Crear directorio temporal único para esta tarea
            tempDir = Files.createTempDirectory("hpc_worker_task_" + UUID.randomUUID().toString().substring(0, 8));
            File scriptFile = new File(tempDir.toFile(), "script.py");
            File inputDataFile = new File(tempDir.toFile(), "input.json");
            File outputDataFile = new File(tempDir.toFile(), "output.json");

            // Escribir el dataset chunk a input.json
            objectMapper.writeValue(inputDataFile, datasetChunk);

            // Código envoltorio o wrapper para ejecutar la función de usuario o script
            // Si el código del usuario contiene `process(item)` o `process_chunk(chunk)`, lo ejecutamos.
            // O si es un script completo que recibe input.json y genera output.json.
            String scriptContent = buildPythonScript(pythonCode, inputDataFile.getAbsolutePath(), outputDataFile.getAbsolutePath());
            try (FileWriter writer = new FileWriter(scriptFile, StandardCharsets.UTF_8)) {
                writer.write(scriptContent);
            }

            System.out.println("[TaskExecutor] Executing Python script in temp directory: " + tempDir.toAbsolutePath());

            String pythonCommand = isWindows() ? "python" : "python3";
            ProcessBuilder pb = new ProcessBuilder(pythonCommand, scriptFile.getAbsolutePath());
            pb.directory(tempDir.toFile());
            pb.redirectErrorStream(true);

            Process process = pb.start();
            String processLogs;
            try (InputStream is = process.getInputStream()) {
                processLogs = new String(is.readAllBytes(), StandardCharsets.UTF_8);
            }

            int exitCode = process.waitFor();
            if (exitCode != 0) {
                System.err.println("[TaskExecutor] Python execution failed (exit code " + exitCode + "):\n" + processLogs);
                throw new RuntimeException("Python script exited with code " + exitCode + ": " + processLogs);
            }

            if (!processLogs.isBlank()) {
                System.out.println("[TaskExecutor] Python stdout/stderr:\n" + processLogs.trim());
            }

            // Leer resultados procesados desde output.json
            if (outputDataFile.exists()) {
                results = objectMapper.readValue(outputDataFile, new TypeReference<List<Object>>() {});
            } else {
                System.err.println("[TaskExecutor] output.json was not created by the Python script.");
            }

        } catch (Exception e) {
            System.err.println("[TaskExecutor] Error executing Python task: " + e.getMessage());
            e.printStackTrace();
        } finally {
            // Limpieza del directorio temporal
            if (tempDir != null) {
                try {
                    Files.walk(tempDir)
                            .map(Path::toFile)
                            .sorted((a, b) -> b.compareTo(a)) // Borrar archivos antes que directorios
                            .forEach(File::delete);
                } catch (Exception ignored) {}
            }
        }

        return results;
    }

    private String buildPythonScript(String userCode, String inputPath, String outputPath) {
        // Envoltorio estándar:
        // Carga input.json en variable `dataset`
        // Si el usuario definió `process(item)`, mapea cada item.
        // Si el usuario definió `process_chunk(chunk)`, le pasa la lista entera.
        // Escribe la lista resultante en output.json.
        return """
import json
import os
import sys

# Ruta de archivos de entrada y salida
INPUT_PATH = r"%s"
OUTPUT_PATH = r"%s"

with open(INPUT_PATH, 'r', encoding='utf-8') as f:
    dataset = json.load(f)

# Código proporcionado por el cliente
%s

# Ejecución del handler
result = []
if 'process_chunk' in locals() and callable(locals()['process_chunk']):
    result = process_chunk(dataset)
elif 'process' in locals() and callable(locals()['process']):
    result = [process(item) for item in dataset]
elif 'transform' in locals() and callable(locals()['transform']):
    result = [transform(item) for item in dataset]
else:
    # Si no se define función explícita, se asume que el usuario dejó la lista final en variable `result`
    if 'result' not in locals():
        result = dataset

with open(OUTPUT_PATH, 'w', encoding='utf-8') as f:
    json.dump(result, f)
""".formatted(inputPath, outputPath, userCode);
    }

    private boolean isWindows() {
        return System.getProperty("os.name").toLowerCase().contains("win");
    }
}
