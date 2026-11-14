# W3DView

W3DView is the model viewer. The contraZH build carries the
[W3DView+](https://github.com/triatomic/w3dviewerPlus) changes listed on
[contraZH Changes](contraZH-Changes.md), and it can be driven by an AI assistant through a Model
Context Protocol (MCP) server.

## MCP server

The assistant reads a model's meshes and materials, opens a model in the viewer, and takes
screenshots of it from chosen sides.

The server has two halves.

* A bridge compiled into W3DView answers each request on the viewer's UI thread.
* A Python program, `tools/w3dview_mcp` in the contraZH repository, speaks MCP over stdio to the
assistant and passes each tool call to the bridge.

The bridge never writes a `.w3d` file. Its only effects on the viewer are loading a model and
moving the camera for a screenshot, and it puts the camera back.

## Requirements

| Need | Detail |
|---|---|
| W3DView | A contraZH build of `W3DViewZH.exe`, running. |
| Python | Python 3 on Windows. The server uses the standard library only, so there is nothing to install. |
| Repository | A checkout of contraZH, because the server runs from `tools/w3dview_mcp`. |
| MCP client | Any client that starts stdio servers. |

## Setup

### 1. Turn the viewer's side on

W3DView's **MCP** menu has two entries.

| Entry | Effect |
|---|---|
| Enable MCP Server | Turns the bridge on or off. It starts on, and the setting is remembered between sessions. |
| Server Information... | Shows the state of the bridge in this viewer. |

With the bridge off, the viewer is hidden from discovery and ignores requests.

### 2. Register the server with the client

The client must start `python -m tools.w3dview_mcp` with the repository root as its working
directory. The repository's `.mcp.json` already holds this entry.

```json
{
  "mcpServers": {
    "generals-w3dview": {
      "command": "python",
      "args": ["-m", "tools.w3dview_mcp"],
      "cwd": "C:\\path\\to\\contraZH"
    }
  }
}
```

Some clients have no working directory field. Give those an absolute Python path and put the
repository root in `PYTHONPATH`.

## Tools

Every tool takes an optional `pid`, the process id of the W3DView to use. It can be left out while
one viewer is running. With several open, call `get_viewer_state` first and pass a `pid` it lists.

| Tool | Arguments | Does |
|---|---|---|
| `get_viewer_state` | none | Lists running viewers that have the bridge on, and the model the selected one displays. |
| `read_model` | `path`, `mesh` | Reads a `.w3d` file's meshes, counts, W3D properties, passes and materials. |
| `open_model` | `path` | Loads a `.w3d` file and displays its top-level object. |
| `capture_views` | `views`, `fit`, `max_size` | Screenshots the displayed model from view cube sides. |

### get_viewer_state

| Field | Meaning |
|---|---|
| `viewers` | Each running viewer's `pid`, window `title` and `bridgeVersion`. |
| `selected.displayedObject` | Name of the object in the viewport, empty when there is none. |
| `selected.sourceFile` | File the object was loaded from. |
| `selected.polygons` | Polygon total of the displayed object. |
| `selected.vertices` | Vertex total of the displayed object. |

The two totals are the ones the `7` key draws in the viewport.

### read_model

`read_model` parses the chunks of the `.w3d` file, so the model does not have to be displayed.

| Argument | Meaning |
|---|---|
| `path` | An absolute `.w3d` path, or a bare file name that W3DView finds through its search paths. Leave it out to read the displayed model. |
| `mesh` | Details only the meshes whose name contains this text. The totals, mesh counts and texture list stay complete. |

The result describes the file first.

| Field | Meaning |
|---|---|
| `file`, `resolvedPath` | The path asked for and the file W3DView opened. |
| `topLevelName` | The HLod name, or the first mesh's name in a file without one. |
| `meshCount` | Number of meshes in the file. |
| `totalPolygons`, `totalVertices` | Sums over every mesh in the file, hidden and collision meshes included. |
| `meshCounts` | `name`, `polygons` and `vertices` of each mesh. |
| `texturesUsed` | Each texture name with the file it resolves to, or `null` when it is missing. |
| `meshes` | One entry per mesh, described below. |

Each mesh has these fields.

| Field | Meaning |
|---|---|
| `name` | `Container.Mesh`. |
| `polygons`, `vertices` | Counts from the mesh header. |
| `w3dProperties` | The W3D export settings, in the next table. |
| `passCount` | Number of material passes. |
| `passes` | One entry per pass. |
| `textures` | Every texture name in the mesh's texture table. |

`w3dProperties` holds what the W3D export options in 3ds Max set.

| Field | Meaning |
|---|---|
| `geometryType` | `NORMAL`, `CAMERA_ALIGNED`, `CAMERA_ORIENTED`, `SKIN`, or an obsolete box or shadow type. |
| `hidden`, `twoSided`, `castShadow` | The Hide, 2-Side and Shadow flags. |
| `collision` | Any of `PHYSICAL`, `PROJECTILE`, `VIS`, `CAMERA`, `VEHICLE`. |
| `shatterable`, `npatchable` | The Shatter and NPatch flags. |
| `prelit` | Lighting wrappers in the mesh, if any. |
| `sortLevel` | Static sort level. |
| `surfaceType` | Surface type number of the first triangle. 13 is the default. |
| `userText` | The object's user properties text from Max. |
| `attributesHex` | The raw attribute bits. |

Each pass has these fields.

| Field | Meaning |
|---|---|
| `vertexMaterial` | The pass's vertex material. |
| `shader` | The pass's shader. |
| `stage0Texture`, `stage1Texture` | The texture of each stage, or `null`. |
| `vertexColors` | Whether the pass carries per-vertex `diffuseAndAlpha`, `diffuseIllumination` or `specular` colours. |
| `otherVertexMaterials`, `otherShaders`, `otherStage0Textures`, `otherStage1Textures` | Present when a per-vertex or per-triangle array assigns more than one entry to the pass. |

A vertex material has these fields.

| Field | Meaning |
|---|---|
| `name` | Material name. |
| `ambient`, `diffuse`, `specular`, `emissive` | Colours as `[r, g, b]`, 0 to 255. |
| `shininess`, `opacity`, `translucency` | The three scalar settings. |
| `specularToDiffuse` | The Specular To Diffuse checkbox. |
| `useDepthCue`, `argbEmissiveOnly`, `depthCueToAlpha` | The remaining attribute flags. |
| `stage0Mapping`, `stage1Mapping` | Each stage's mapping `type`, its `args` text and its `uvChannel`. |

A mapping `type` is one of `UV`, `ENVIRONMENT`, `CHEAP_ENVIRONMENT`, `SCREEN`, `LINEAR_OFFSET`,
`SILHOUETTE`, `SCALE`, `GRID`, `ROTATE`, `SINE_LINEAR_OFFSET`, `STEP_LINEAR_OFFSET`,
`ZIGZAG_LINEAR_OFFSET`, `WS_CLASSIC_ENV`, `WS_ENVIRONMENT`, `GRID_CLASSIC_ENV`, `GRID_ENVIRONMENT`,
`RANDOM`, `EDGE`, `BUMPENV`, `GRID_WS_CLASSIC_ENV` or `GRID_WS_ENVIRONMENT`. The `args` text is the
mapper argument string exactly as the file stores it, for example `UPerSec=0.5`.

A shader has these fields.

| Field | Values |
|---|---|
| `srcBlend` | `ZERO`, `ONE`, `SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA` |
| `destBlend` | `ZERO`, `ONE`, `SRC_COLOR`, `ONE_MINUS_SRC_COLOR`, `SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA`, `SRC_COLOR_PREFOG` |
| `depthWrite`, `alphaTest`, `texturing`, `secGradient` | `true` or `false` |
| `depthCompare` | `PASS_NEVER`, `PASS_LESS`, `PASS_EQUAL`, `PASS_LEQUAL`, `PASS_GREATER`, `PASS_NOTEQUAL`, `PASS_GEQUAL`, `PASS_ALWAYS` |
| `priGradient` | `DISABLE`, `MODULATE`, `ADD`, `BUMPENVMAP`, `BUMPENVMAPLUMINANCE`, `MODULATE2X` |
| `detailColorFunc`, `postDetailColorFunc` | `DISABLE`, `DETAIL`, `SCALE`, `INVSCALE`, `ADD`, `SUB`, `SUBR`, `BLEND`, `DETAILBLEND`, `ADDSIGNED`, `ADDSIGNED2X`, `SCALE2X`, `MODALPHAADDCOLOR` |
| `detailAlphaFunc`, `postDetailAlphaFunc` | `DISABLE`, `DETAIL`, `SCALE`, `INVSCALE` |

A texture has these fields.

| Field | Meaning |
|---|---|
| `name` | File name as the model stores it. |
| `resolvedPath` | The file W3DView found, or `null`. |
| `publish`, `noLod`, `clampU`, `clampV`, `alphaBitmap`, `bumpmap` | The texture flags. |
| `mipLevels` | `ALL`, `2`, `3` or `4`. |
| `hint` | `BASE`, `EMISSIVE`, `ENVIRONMENT` or `SHINY_MASK`. |
| `animType`, `frameCount`, `frameRate` | Animation settings. `animType` is `LOOP`, `PINGPONG`, `ONCE` or `MANUAL`. |

A texture without the optional info chunk reports `"settings": "defaults"` in place of the flag and
animation fields.

W3DView looks for a texture in its search paths first and beside the model second. It tries the
stored name and then the same name with a `.dds` extension, as the game does.

#### UV channels

A `.w3d` file stores texture coordinates per pass and stage. It does not store the 3ds Max map
channel number. `uvChannel` numbers the mesh's distinct coordinate sets from 1 in file order, so two
stages with the same number were exported from one channel. A stage without coordinates reports
`null`.

### open_model

`open_model` takes an absolute `path`. It loads the file as File > Open does, selects the top-level
object in the asset tree, and returns the same fields as `selected` in `get_viewer_state`.

### capture_views

| Argument | Default | Meaning |
|---|---|---|
| `views` | `front-left-top`, `front`, `left`, `top` | List of view cube regions to capture. |
| `fit` | `true` | Frames the whole model. `false` keeps the current zoom and orbit centre. |
| `max_size` | 1024 | Longest side of each returned image, in pixels. |

A view is a face of the view cube, or two or three faces joined by `-` for an edge or a corner.

| Face | Looks from |
|---|---|
| `front` | +X |
| `back` | -X |
| `left` | -Y |
| `right` | +Y |
| `top` | +Z |
| `bottom` | -Z |

`back-right` is an edge and `front-left-top` is a corner. A view cannot name both faces of one axis.

Each view puts the camera where a click on that part of the view cube ends, without the turn
animation. The tool returns one PNG per view and also saves it as `<object>-<view>.png` in
`%TEMP%\w3dview-mcp`. A later capture of the same object and view replaces the file.

The capture is the viewport as drawn, so it includes the view cube, the `7` key totals and any
overlay that is switched on. The camera returns to where it was after the last view.

## Limits

| Limit | Value |
|---|---|
| Request size | 64 KiB |
| Time for one command | 30 seconds |
| Image shrink | By a whole factor, nearest pixel |

## Safety

* The bridge opens no network port. It is desktop automation between two programs of the same
Windows user, so untrusted programs should not run in that desktop session.
* A request can make W3DView read any `.w3d` file the user can read, and write a response file
beside the request file and a `.bmp` capture at the path the request names. The Python server keeps
both in `%TEMP%\w3dview-mcp`.
* Turning **Enable MCP Server** off stops all of it.
