# MetalFarmMania

MetalFarmMania is a UE4SS C++ mod for Subnautica 2 that adds configurable Metal Farm recipes.

You define entries in a JSONC config file (comments supported), and the mod patches Metal Farm behavior at runtime.

## What It Does

- Adds custom Metal Farm inputs and outputs from config.
- Supports multiple possible outputs per entry.
- Supports drop chance and stack yield per output.
- Supports growth speed presets: slow, medium, fast.
- Supports custom per-entry growth time in seconds.

## Requirements

- Subnautica 2
- UE4SS installed for the game
- MetalFarmMania mod files

## Installation 

1. Install UE4SS for Subnautica 2 first.
2. Copy the MetalFarmMania mod folder into your UE4SS/Mods folder.
3. Confirm these paths exist:

```text
<Subnautica2>/.../ue4ss/Mods/MetalFarmMania/dlls/main.dll
<Subnautica2>/.../ue4ss/Mods/MetalFarmMania/enabled.txt
```

4. Edit this config file:

```text
<Subnautica2>/.../ue4ss/Mods/MetalFarmMania/MetalFarmAdditions.jsonc
```

## Configuration

### Root Format

```jsonc
{
  "schemaVersion": 2,
  "entries": [
    {
      "id": "unique_entry_id",
      "inputItemPath": "/Game/.../DA_SomeItemType.DA_SomeItemType_C",
      "growthSpeed": "medium",
      "growthTimeSeconds": 180.0,
      "outputs": [
        {
          "resourceClassPath": "/Game/.../BP_SomeResource.BP_SomeResource_C",
          "yield": 2,
          "dropChance": 1.0
        }
      ]
    }
  ]
}
```

### Entry Fields

- id (required, string)
  - Unique identifier for this recipe entry.
- inputItemPath (required, string)
  - ItemType object path for the item inserted into the Metal Farm.
- growthSpeed (optional, string)
  - One of: slow, medium, fast.
  - Default: medium.
- growthTimeSeconds (optional, number)
  - If present, must be greater than 0.
  - Overrides growthSpeed with a custom timer.
- outputs (required, array)
  - At least one output object is required.

### Output Fields

- resourceClassPath (required, string)
  - Resource actor/class object path spawned as output.
- yield (required, integer)
  - Must be greater than 0.
- dropChance (optional, number)
  - Range: 0.0 to 1.0.
  - Default: 1.0.

## Example

```jsonc
{
  "schemaVersion": 2,
  "entries": [
    {
      "id": "celestine",
      "inputItemPath": "/Game/Data/ItemType/Tools/DA_Flares_ItemType.DA_Flares_ItemType_C",
      "growthSpeed": "medium",
      "outputs": [
        {
          "resourceClassPath": "/Game/Blueprints/Items/Resources/BP_Celestine.BP_Celestine_C",
          "yield": 3,
          "dropChance": 1.0
        },
        {
          "resourceClassPath": "/Game/Blueprints/Items/Resources/BP_Celestine.BP_Celestine_C",
          "yield": 1,
          "dropChance": 0.5
        }
      ]
    }
  ]
}
```

## Troubleshooting

- Mod does not appear to load:
  - Verify UE4SS is installed correctly.
  - Verify enabled.txt exists for the mod.
  - Verify main.dll is in the mod dlls folder.
- Config not applied:
  - Verify file path is exactly Mods/MetalFarmMania/MetalFarmAdditions.jsonc.
  - Verify schemaVersion is 2 (or omit it).
  - Verify each entry has id, inputItemPath, and valid outputs.
- Parse/validation errors:
  - Check for invalid number types, missing required fields, or dropChance outside 0.0 to 1.0.