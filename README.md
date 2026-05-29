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
      "inputItem": "Flares",
      "growthSpeed": "medium",
      "growthTimeSeconds": 180.0,
      "outputs": [
        {
          "outputItem": "Celestine",
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
- inputItem (required, string)
  - Friendly token or ItemType object path for the item inserted into the Metal Farm.
  - Legacy alias `inputItemPath` is still supported.
- growthSpeed (optional, string)
  - One of: slow, medium, fast.
  - Default: medium.
- growthTimeSeconds (optional, number)
  - Optional custom grow time in seconds; if present, must be greater than 0.
  - Takes priority over growthSpeed when both are set.
- outputs (required, array)
  - At least one output object is required.

### Output Fields

- outputItem (required, string)
  - Friendly token or resource actor/class object path spawned as output.
  - Legacy alias `resourceClassPath` is still supported.
  - Friendly-name mode is resolved against loaded objects under `/Game/Blueprints/Items/`.

Friendly-name matching rule:

- The resolver prefers the token between the first and last underscore in the asset name.
- Example input item assets:
  - `DA_OxygenTank_Small_ItemType` -> use `OxygenTank_Small`
  - `DA_OxygenTank_Medium_EquippableItemType` -> use `OxygenTank_Medium`
- This avoids overlap where multiple assets share a broader substring such as `Fins`.
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
      "inputItem": "Flares",
      "growthSpeed": "medium",
      "growthTimeSeconds": 210.0,
      "outputs": [
        {
          "outputItem": "Celestine",
          "yield": 3,
          "dropChance": 1.0
        },
        {
          "outputItem": "/Game/Blueprints/Items/Resources/BP_Celestine.BP_Celestine_C",
          "yield": 1,
          "dropChance": 0.5
        }
      ]
    },
    {
      "id": "legacy_inputpath_example",
      "inputItemPath": "/Game/Blueprints/Items/Equipment/Fins/DA_Fins_ItemType.DA_Fins_ItemType",
      "growthSpeed": "slow",
      "growthTimeSeconds": 300.0,
      "outputs": [
        {
          "outputItem": "CopperOre",
          "yield": 1,
          "dropChance": 1.0
        }
      ]
    }
  ]
}
```

## Multiplayer Notes

- Only the host is required to have this mod for Metal Farms to produce the correct custom outputs.
- With host-only install, only the host can insert custom configured input items into Metal Farms and see growth timers.
- Any player can harvest the custom outputs once they are produced.
- If other players also want to insert custom input items and see growth timers, they must install the mod too as both the item filter for the metal farms and timers are client-side

## Troubleshooting

- Mod does not appear to load:
  - Verify UE4SS is installed correctly.
  - Verify enabled.txt exists for the mod.
  - Verify main.dll is in the mod dlls folder.
- Config not applied:
  - Verify file path is exactly Mods/MetalFarmMania/MetalFarmAdditions.jsonc.
  - Verify schemaVersion is 2 (or omit it).
  - Verify each entry has id, inputItem/inputItemPath, and valid outputs with outputItem/resourceClassPath.
- Parse/validation errors:
  - Check for invalid number types, missing required fields, or dropChance outside 0.0 to 1.0.
- Friendly-name resolution picked the wrong item:
  - Use the full object path to force an exact asset.
  - Exact base-name matches are preferred automatically (for example `Fins` resolves to `DA_Fins_ItemType`, not `DA_ImprovedFins_ItemType`).

## Known Bugs

- Items that share the same gameplay tag can sometimes be inserted into a Metal Farm slot even when they are not the exact configured recipe input.
- A full fix for this behavior has not been found yet.
- If the inserted item is not exactly the configured input item, the Metal Farm will not progress or grow.
- Example: if a recipe is configured for regular Fins, Improved Fins can still be inserted because of shared tags, but the farm will only grow when regular Fins are inserted.