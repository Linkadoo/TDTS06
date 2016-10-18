import javax.swing.*;

public class RouterNode {
  private int myID;
  private GuiTextArea myGUI;
  private RouterSimulator sim;
  private int[] costs = new int[RouterSimulator.NUM_NODES];
  private int[][] distanceTable =
        new int[RouterSimulator.NUM_NODES][RouterSimulator.NUM_NODES];

  private int[] neighbours;
  //--------------------------------------------------
  public RouterNode(int ID, RouterSimulator sim, int[] costs) {
    myID = ID;
    this.sim = sim;
    myGUI =new GuiTextArea("  Output window for Router #"+ ID + "  ");

    System.arraycopy(costs, 0, this.costs, 0, RouterSimulator.NUM_NODES);


    //Calculate who's this nodes neighbours
    calcNeighbours();

    //Initialize distance table
    for(int i = 0; i< RouterSimulator.NUM_NODES; i++)
      for(int j = 0; j< RouterSimulator.NUM_NODES; j++)
        distanceTable[i][j] = RouterSimulator.INFINITY;
    distanceTable[ID] = costs;
    printDistanceTable();
    //Send distance table to all neighbours
    broadcastUpdate();
  }

  //--------------------------------------------------
  public void recvUpdate(RouterPacket pkt) {
    int source = pkt.sourceid;
    int[] mincost = pkt.mincost;
    int[] myPreviousDistances = new int[distanceTable[myID].length];
    int[] myNewDistances= new int[distanceTable[myID].length];

    distanceTable[source] = mincost;
    System.arraycopy(distanceTable[myID], 0,
          myPreviousDistances, 0, distanceTable[myID].length);
    updateDistances();
    System.arraycopy(distanceTable[myID], 0,
          myNewDistances, 0, distanceTable[myID].length);

          String line = "";
          for(int j = 0; j< RouterSimulator.NUM_NODES; j++){
          line += "\t" + myPreviousDistances[j];

          }
          myGUI.println(line);

          line = "";
          for(int j = 0; j< RouterSimulator.NUM_NODES; j++){
          line += "\t" + myNewDistances[j];

          }
          myGUI.println(line);
    if(!isEqualArrays(myPreviousDistances, myNewDistances)){
      broadcastUpdate();
      myGUI.println("The distances are not equal!!");
    }

  }

  private void updateDistances(){
    int cheapestCost;
    int pathCost;
    for(int i=0; i < RouterSimulator.NUM_NODES; i++){
      //No need to check the cost to ourself
      cheapestCost = distanceTable[myID][i];
      if(myID == i)
        continue;

      for(int j=0; j<neighbours.length; j++){
        pathCost = costs[neighbours[j]] + distanceTable[neighbours[j]][i];
        if(pathCost < cheapestCost)
          cheapestCost = pathCost;
      }
      distanceTable[myID][i] = cheapestCost;
    }
  }

  private void calcNeighbours(){
    int[] neighboursTemp = new int[RouterSimulator.NUM_NODES];
    int neighIndex = 0;
    for(int i=0; i < RouterSimulator.NUM_NODES; i++)
      if((i != myID) && (costs[i] != RouterSimulator.INFINITY))
        neighboursTemp[neighIndex++] = i;

    neighbours = new int[neighIndex];
    System.arraycopy(neighboursTemp, 0, neighbours, 0, neighIndex);
  }
  //--------------------------------------------------
  private void sendUpdate(RouterPacket pkt) {
    sim.toLayer2(pkt);
  }

  private void broadcastUpdate(){
    for (int i = 0; i<neighbours.length; i++){
      RouterPacket packet =
          new RouterPacket(myID, neighbours[i], distanceTable[myID]);
      sendUpdate(packet);
    }
  }

  //--------------------------------------------------
  public void printDistanceTable() {
	  myGUI.println("Current table for " + myID +
			"  at time " + sim.getClocktime() + "\n");

    myGUI.println("Distancetable:");

    String line;
    for(int i = 0; i< RouterSimulator.NUM_NODES; i++){
      line = "";
      for(int j = 0; j< RouterSimulator.NUM_NODES; j++){
      line += "\t" + distanceTable[i][j];

      }
      myGUI.println(line);
    }

  }

  //--------------------------------------------------
  public void updateLinkCost(int dest, int newcost) {
  }

  private boolean isEqualArrays(int[] arr1, int[] arr2){
    if(!(arr1.length == arr2.length))
      return false;
    for(int i =0; i<arr1.length; i++){
      if(arr1[i] != arr2[i])
        return false;
    }
    return true;
  }

}
