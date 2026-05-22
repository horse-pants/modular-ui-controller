# ArduinoJson v7 Rules

## API differences from v6

```cpp
// ✅ Correct (v7)
obj["key"].is<T>()
obj["key"].isNull()
JsonObjectConst obj = doc.as<JsonObjectConst>();

// ❌ Wrong (v6 API — no longer exists)
obj.containsKey()
doc.as<JsonObject>()      // use JsonObjectConst when reading
```

## Performance

- **Filter unused fields** when parsing large incoming WebSocket messages — use `DeserializationOption::Filter`.
- Reuse a single `JsonDocument` per handler when feasible; don't allocate per-message in hot paths.
