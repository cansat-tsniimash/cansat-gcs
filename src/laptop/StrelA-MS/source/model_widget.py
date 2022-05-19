import os

from PyQt5 import QtGui
import pyqtgraph as PG
import pyqtgraph.opengl as OpenGL
import numpy as NumPy
from stl import mesh as StlMesh
from itertools import chain
import struct

from source import settings_control
from source import RES_ROOT

MESH_PATH = os.path.join(RES_ROOT, "models/CanCubeSat-2-for-GKS.stl")
MESH_COLOR_PATH = os.path.join(RES_ROOT, "models/CanCubeSat-2-for-GKS_color.mfcl")
SCENE_MESH_PATH = os.path.join(RES_ROOT, "models/axis.stl")
SCENE_MESH_COLOR_PATH = os.path.join(RES_ROOT, "models/axis.mfcl")

class ModelWidget(OpenGL.GLViewWidget):
    class ModelObject(OpenGL.GLMeshItem())
        MOTION_NO_MOTION = 0
        MOTION_ROTATION_QUAT = 2
        def __init__(self, *args, **kwargs):
            super(ModelWidget, self).__init__(args, kwargs)
            self.action_mode = self.MOTION_NO_MOTION

        def set_action_mode(self, mode=self.MOTION_NO_MOTION):
            self.action_mode = mode

        def get_action_mode(self):
            return self.action_mode

        def set_sourse_id(self, sourse_id):
            self.sourse_id = sourse_id

        def set_message_id(self, message_id):
            self.message_id = message_id

        def set_data_fields_id(self, data_fields_id):
            self.data_fields_id = data_fields_id

        def get_sourse_id(self):
            return self.sourse_id

        def get_message_id(self):
            return self.message_id

        def get_data_fields_id(self):
            return self.data_fields_id

    def __init__(self):
        super(ModelWidget, self).__init__()
        self.settings = settings_control.init_settings()

        self.setBackgroundColor(self.settings.value("CentralWidget/ModelWidget/background_color"))

        self.setup_ui()
        self.setup_ui_design()
        self.update_current_values()

    def setup_ui(self):
        self.gird = OpenGL.GLGridItem()
        self.addItem(self.gird)

        self.axis = OpenGL.GLAxisItem()
        self.addItem(self.axis)

        self.mesh_list = []OpenGL.GLMeshItem()
        #self.addItem(self.mesh)

        self.scene = OpenGL.GLMeshItem()
        self.addItem(self.scene)

    def setup_ui_design(self):
        for mesh in self.mesh_list:
            self.removeItem(mesh)
        self.mesh_list = []

        self.settings.beginGroup("CentralWidget/ModelWidget/Grid")
        if self.settings.value("is_on") != False:
            self.gird.show()
            self.gird.scale(*self.settings.value("scale"))
            self.gird.translate(*self.settings.value("translate"))
        else:
            self.gird.hide()
        self.settings.endGroup()

        self.settings.beginGroup("CentralWidget/ModelWidget/Axis")
        if self.settings.value("is_on") != False:
            self.axis.show()
            self.axis.scale(*self.settings.value("scale"))
            self.axis.translate(*self.settings.value("translate"))
        else:
            self.axis.hide()
        self.settings.endGroup()

        self.settings.beginGroup("CentralWidget/ModelWidget/Meshes")
        for group in self.settings.childGroups():
            if self.settings.value(group + "/is_on") != False:
                self.settings.beginGroup(group)
                mesh = ModelWidget.ModelObject()
                model_color = None
                try:
                    if self.settings.value("path") == "Default":
                        verts = self._get_mesh_points(MESH_PATH)
                    else:
                        verts = self._get_mesh_points(self.settings.value("path"))
                    if self.settings.value("Colors/is_on") != False:
                        if self.settings.value("path") == "Colors/path":
                            model_color = self._get_face_colors(self.settings.value("Colors/path"))
                        else:
                            model_color = self._get_face_colors(MESH_COLOR_PATH)
                except Exception:
                    pass
                else:
                    faces = NumPy.array([(i, i + 1, i + 2,) for i in range(0, len(verts), 3)])

                    mesh.setMeshData(vertexes=verts,
                                     faces=faces, 
                                     faceColors=model_color,
                                     edgeColor=(0, 0, 0, 1),
                                     drawEdges=self.settings.value("draw_edges"), 
                                     drawFaces=self.settings.value("draw_faces"),
                                     smooth=self.settings.value("smooth"), 
                                     shader=self.settings.value("shader"), 
                                     computeNormals=self.settings.value("compute_normals"))
                    mesh.meshDataChanged()

                    if self.settings.value("Rotation/is_on") != False:
                        self.settings.beginGroup("Rotation/Packet")
                        mesh.set_action_mode(mesh.MOTION_ROTATION_QUAT)
                        mesh.set_sourse_id(self.settings.value("sourse_id"))
                        mesh.set_message_id(self.settings.value("message_id"))
                        mesh.set_data_fields_id(self.settings.value("quat_field_id"))
                        self.settings.endGroup()

                    self.mesh_list.append(mesh)
                    self.addItem(mesh)

                self.settings.endGroup()
        self.settings.endGroup()

        self.settings.beginGroup("CentralWidget/ModelWidget/Camera")
        self.setCameraPosition(distance=self.settings.value("distance"),
                               elevation=self.settings.value("elevation"),
                               azimuth=self.settings.value("azimuth"))
        self.pan(*self.settings.value("pan"))
        self.settings.endGroup()
        
        self.sourse_id = self.settings.value("CentralWidget/ModelWidget/sourse_id")
        self.message_id = self.settings.value("CentralWidget/ModelWidget/message_id")
        self.quat_field_id = self.settings.value("CentralWidget/ModelWidget/quat_field_id")

    def update_current_values (self):
        pass

    def _get_face_colors(self, color_path):
        color_file = open(color_path, 'rb')
        bin_data = color_file.read()
        color_file.close()

        color = NumPy.ndarray(shape=(len(bin_data) // 16, 4,))
        for i in range(0, len(bin_data), 16):
            color[i // 16] = struct.unpack(">4f", bin_data[i: i + 16])
    
        return color

    def _get_mesh_points(self, mesh_path):
        mesh = StlMesh.Mesh.from_file(mesh_path)
        points = mesh.points
        points = NumPy.array(list(chain(*points)))
        nd_points = NumPy.ndarray(shape=(len(points) // 3, 3,))
        for i in range(0, len(points) // 3):
            nd_points[i] = points[i * 3: (i + 1) * 3]
        return nd_points

    def new_data_reaction(self, data):
        for mesh in self.mesh_list:
            if mesh.get_action_mode() == mesh.MOTION_ROTATION_QUAT:
                for msg in data[::-1]:
                    if (msg.get_source_id() == mesh.get_source_id()) and (msg.get_message_id() == mesh.get_message_id()):
                        quat = []
                        for i in range(4):
                            quat.append(msg.get_data_dict().get(mesh.get_data_fields_id()[i], None))
                            if quat[-1] is None:
                                quat = None
                                break
                        if quat is not None:
                            quat = QtGui.QQuaternion(*quat)
                            self.clear_data()
                            self._rotate_object(mesh, *quat.getAxisAndAngle())
                            break

    def _rotate_object(self, obj, axis, angle):
        obj.rotate(angle, axis[0], axis[1], axis[2])

    def clear_data(self):
        self.mesh.resetTransform()