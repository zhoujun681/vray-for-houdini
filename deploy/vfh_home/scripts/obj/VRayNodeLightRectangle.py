# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import hou

# The node is passed in kwargs['node']
node = kwargs['node']

node_parm = node.parm("dimmer")
node_parm.setExpression("if(ch(\"./enabled\"), ch(\"./intensity\"), 0)")

linenode = node.createNode("line")
node_parm = linenode.parm("diry")
node_parm.set(0)
node_parm = linenode.parm("dirz")
node_parm.set(-1)

gridnode = node.createNode("grid")
node_parm = gridnode.parm("orient");
node_parm.set("xy")
node_parm = gridnode.parm("sizex");
node_parm.setExpression("ch(\"../u_size\")")
node_parm = gridnode.parm("sizey");
node_parm.setExpression("ch(\"../v_size\")")
node_parm = gridnode.parm("rows");
node_parm.set(2)
node_parm = gridnode.parm("cols");
node_parm.setExpression("ch(\"rows\")")

mergenode = node.createNode("merge")
mergenode.setInput(0, gridnode, 0)
mergenode.setInput(1, linenode, 0)

mergenode.setRenderFlag(True)
mergenode.setDisplayFlag(True)

node.layoutChildren()
node.setName("vraylightrectangle")