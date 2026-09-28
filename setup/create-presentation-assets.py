"""Author five editable faction fleet meshes in Blender for the combat presentation.

These are original procedural silhouette studies, not extracted franchise meshes
or finished hero assets. Export retains separate material slots for hull/windows.
Run with Blender --background --python this_file.py.
"""
from pathlib import Path
import math
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "game" / "ArtSource" / "FleetStudies"
OUTPUT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, color, metal=0.5, roughness=0.32, emission=0):
    result = bpy.data.materials.new(name)
    result.use_nodes = True
    node = result.node_tree.nodes.get("Principled BSDF")
    node.inputs["Base Color"].default_value = (*color, 1)
    node.inputs["Metallic"].default_value = metal
    node.inputs["Roughness"].default_value = roughness
    node.inputs["Emission Color"].default_value = (*color, 1)
    node.inputs["Emission Strength"].default_value = emission
    return result


HULL = material("Federation_Alloy", (0.38, 0.47, 0.55))
PANEL = material("Federation_Panels", (0.14, 0.22, 0.29))
GREEN = material("Klingon_Armor", (0.10, 0.24, 0.13))
ROMULAN = material("Romulan_Armor", (0.12, 0.34, 0.20))
BRONZE = material("Cardassian_Alloy", (0.44, 0.29, 0.10))
COPPER = material("Ferengi_Alloy", (0.46, 0.19, 0.07))
DARK = material("Recesses", (0.028, 0.042, 0.055), roughness=0.65)
BLUE = material("Impulse_Blue", (0.10, 0.55, 1), metal=0, emission=5)
RED = material("Collector_Red", (1, 0.08, 0.025), metal=0, emission=3)
JADE = material("Disruptor_Green", (0.18, 1, 0.28), metal=0, emission=4)
WINDOW = material("Windows", (0.8, 0.85, 0.65), metal=0, emission=2)


def finish(obj, name, mat):
    obj.name = name
    obj.data.materials.append(mat)
    for face in obj.data.polygons:
        face.use_smooth = True
    return obj


def ellipsoid(name, location, scale, mat, segments=32):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=16, location=location)
    obj = bpy.context.object
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, name, mat)


def strut(name, start, end, radius, mat, vertices=16):
    a, b = Vector(start), Vector(end)
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=(b-a).length, location=(a+b)/2)
    obj = bpy.context.object
    obj.rotation_euler = (b-a).to_track_quat("Z", "Y").to_euler()
    return finish(obj, name, mat)


def plate(name, points, thickness, mat):
    n = len(points)
    vertices = [(x, y, z+thickness/2) for x,y,z in points] + [(x,y,z-thickness/2) for x,y,z in points]
    faces = [tuple(range(n)), tuple(reversed(range(n,2*n)))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    return obj


def windows(center, radii, height, count=28):
    for index in range(count):
        angle = 2*math.pi*index/count
        ellipsoid("Window", (center[0]+radii[0]*math.cos(angle), center[1]+radii[1]*math.sin(angle), height),
                  (0.035, 0.023, 0.015), WINDOW, segments=8)


def federation():
    ellipsoid("Saucer", (0.95,0,0.35), (1.12,1.28,0.18), HULL)
    ellipsoid("SaucerInset", (0.95,0,0.505), (0.90,1.02,0.025), PANEL)
    ellipsoid("Bridge", (0.85,0,0.56), (0.23,0.25,0.09), HULL)
    ellipsoid("Engineering", (-0.7,0,-0.25), (1.12,0.38,0.32), HULL)
    ellipsoid("Deflector", (0.38,0,-0.25), (0.06,0.25,0.20), BLUE)
    strut("Neck", (-0.2,0,-0.2), (0.4,0,0.34), 0.13, HULL)
    for side in (-1,1):
        strut("Pylon", (-0.8,0,-0.1), (-0.65,side*1.12,0.24), 0.09, HULL)
        ellipsoid("Nacelle", (-0.9,side*1.12,0.27), (1.12,0.16,0.17), HULL)
        ellipsoid("BussardCollector", (0.15,side*1.12,0.27), (0.11,0.145,0.145), RED)
        strut("WarpGrille", (-1.75,side*1.255,0.28), (-0.05,side*1.255,0.28), 0.035, BLUE)
    windows((0.95,0), (1.03,1.18), 0.42)


def klingon():
    ellipsoid("CommandHead", (1.7,0,0.04), (0.42,0.43,0.19), GREEN)
    ellipsoid("Neck", (0.55,0,0), (1.05,0.17,0.12), GREEN)
    ellipsoid("EngineBody", (-0.65,0,0.12), (0.8,0.62,0.28), GREEN)
    for side in (-1,1):
        plate("SweptWing", [(-0.7,side*0.35,0.12),(-1.3,side*1.7,-0.10),(0.0,side*2.05,-0.35),(0.5,side*1.8,-0.32)], 0.09, GREEN)
        strut("WingSpine", (-0.7,side*.4,.15), (-.8,side*1.65,-.12), .065, DARK)
        strut("Disruptor", (-0.05,side*1.95,-.3), (.7,side*1.95,-.3), .075, GREEN)
        ellipsoid("DisruptorTip", (.71,side*1.95,-.3), (.08,.055,.055), JADE, 12)
    ellipsoid("Impulse", (-1.4,0,.12), (.05,.35,.12), RED)


def romulan():
    ellipsoid("Head", (1.65,0,.12), (.52,.66,.23), ROMULAN)
    ellipsoid("UpperHull", (-.3,0,.63), (1.35,.62,.18), ROMULAN)
    ellipsoid("LowerHull", (-.45,0,-.45), (1.16,.48,.14), ROMULAN)
    for side in (-1,1):
        plate("UpperWing", [(1.65,side*.3,.1),(.15,side*1.48,.28),(-1.65,side*1.1,.47),(-1.2,side*.35,.65)], .10, ROMULAN)
        plate("LowerWing", [(1.15,side*.35,-.05),(.05,side*1.34,-.05),(-1.4,side*.9,-.34),(-.7,side*.3,-.47)], .07, ROMULAN)
        ellipsoid("Nacelle", (-.3,side*1.36,.12), (1.08,.17,.2), ROMULAN)
        strut("PlasmaGrille", (-1.1,side*1.49,.13), (.5,side*1.49,.13), .04, JADE)
    ellipsoid("ForwardEmitter", (2.13,0,.1), (.045,.2,.07), JADE)


def cardassian():
    ellipsoid("ForwardHull", (1.1,0,.05), (.8,.85,.16), BRONZE)
    ellipsoid("Spine", (-.3,0,.05), (1.12,.28,.22), BRONZE)
    ellipsoid("CommandDeck", (.9,0,.25), (.38,.34,.08), BRONZE)
    for side in (-1,1):
        plate("AftBlade", [(.0,side*.2,0),(-.65,side*.95,.04),(-1.65,side*.9,.1),(-1.0,side*.28,.13)], .10, BRONZE)
        strut("AftEngine", (-1.5,side*.62,.1), (-.75,side*.62,.1), .10, DARK)
        ellipsoid("EngineGlow", (-1.52,side*.62,.1), (.04,.08,.08), RED, 12)
    strut("CentralTail", (-2.0,0,.05), (-.6,0,.05), .13, BRONZE)
    ellipsoid("SpiralEmitter", (1.83,0,.05), (.045,.18,.10), RED)
    windows((1.1,0),(.67,.72),.17,20)


def ferengi():
    for side in (-1,1):
        points=[]
        for index in range(9):
            angle=math.radians(30+index*18)
            points.append((.1+1.1*math.cos(angle),side*1.35*math.sin(angle),.03))
        for index in reversed(range(9)):
            angle=math.radians(30+index*18)
            points.append((.1+.68*math.cos(angle),side*.89*math.sin(angle),.03))
        plate("MarauderArc", points,.32,COPPER)
        ellipsoid("ForwardClaw", (1.0,side*.62,.02), (.55,.27,.20), COPPER)
        ellipsoid("WeaponBank", (1.49,side*.62,.02), (.055,.16,.1), RED, 12)
    ellipsoid("RearHull", (-.85,0,.08), (.55,.91,.28), COPPER)
    ellipsoid("Bridge", (-.4,0,.34), (.35,.4,.15), COPPER)
    ellipsoid("Impulse", (-1.38,0,.06), (.035,.45,.12), BLUE)


for name, build in [("Federation",federation),("Klingon",klingon),("Romulan",romulan),("Cardassian",cardassian),("Ferengi",ferengi)]:
    bpy.ops.object.select_all(action="DESELECT")
    before=set(bpy.data.objects)
    build()
    objects=[obj for obj in bpy.data.objects if obj not in before and obj.type=="MESH"]
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.join()
    mesh=bpy.context.object
    mesh.name="SM_"+name
    bpy.context.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.export_scene.gltf(filepath=str(OUTPUT/(name+".glb")),export_format="GLB",use_selection=True)
    print("BOTF_FLEET_MESH",name,len(mesh.data.vertices),len(mesh.data.polygons))
    mesh.hide_set(True)
    mesh.select_set(False)

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT/"FleetStudies.blend"))
print("BOTF_FLEET_STUDIES_COMPLETE",OUTPUT)
