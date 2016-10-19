import javax.swing.*;
import java.util.Arrays;

public class RouterNode {
  private boolean pReverse = true;
  private int myID;
  private GuiTextArea myGUI;
  private RouterSimulator sim;
  private int[] costs = new int[RouterSimulator.NUM_NODES];
  private int[][] distanceTable =
        new int[RouterSimulator.NUM_NODES][RouterSimulator.NUM_NODES];
  private int[] route = new int[RouterSimulator.NUM_NODES];

  private int[] neighbours;
  //--------------------------------------------------
  public RouterNode(int ID, RouterSimulator sim, int[] costs) {
    myID = ID;
    this.sim = sim;
    myGUI =new GuiTextArea("  Output window for Router #"+ ID + "  ");

    System.arraycopy(costs, 0, this.costs, 0, RouterSimulator.NUM_NODES);

    Arrays.fill(route, RouterSimulator.INFINITY);
    //Calculate this node's neighbours and init route array.
    calcNeighbours();
    initRoute();

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

    //If a change has occured, broadcast it
    if(!isEqualArrays(myPreviousDistances, myNewDistances))
      broadcastUpdate();
  }

  private void updateDistances(){
    int cheapestCost;
    int pathCost;
    for(int i=0; i < RouterSimulator.NUM_NODES; i++){

      //Set the default cheapest cost to what we know from our
      //distance table (before this update).
      cheapestCost = costs[i];
      if(isNeighbour(i))
        route[i] = i;

      //No need to check the cost to ourself
      if(myID == i){
        route[i] = myID;
        continue;
      }

      //Iterate through all neighbours and compare their DV table to
      //find the cheapest path.
      for(int j=0; j<neighbours.length; j++){
        pathCost = costs[neighbours[j]] + distanceTable[neighbours[j]][i];
        if(pathCost < cheapestCost){
          cheapestCost = pathCost;
          route[i] = neighbours[j];
        }
      }

      distanceTable[myID][i] = cheapestCost;
    }
  }

  private boolean isNeighbour(int ID){
    for(int i = 0; i < neighbours.length; i++){
      if(neighbours[i] == ID)
        return true;
    }
    return false;
  }
  //Initiates an array of neighbours.

  private void calcNeighbours(){
    int[] neighboursTemp = new int[RouterSimulator.NUM_NODES];
    int neighIndex = 0;
    for(int i=0; i < RouterSimulator.NUM_NODES; i++)
      if((i != myID) && (costs[i] != RouterSimulator.INFINITY))
        neighboursTemp[neighIndex++] = i;

    neighbours = new int[neighIndex];
    System.arraycopy(neighboursTemp, 0, neighbours, 0, neighIndex);
  }

  //Initiates the route array. Must be done after calcNeighbours()!
  private void initRoute(){
    //Set route array to what's known.
    for(int i=0; i<neighbours.length; i++){
      route[neighbours[i]] = neighbours[i];
    }
  }
  //--------------------------------------------------
  private void sendUpdate(RouterPacket pkt) {
    sim.toLayer2(pkt);
  }

  private void broadcastUpdate(){
    for (int i = 0; i<neighbours.length; i++){
      int[] distanceVector = new int[RouterSimulator.NUM_NODES];
      System.arraycopy(distanceTable[myID], 0, distanceVector, 0, RouterSimulator.NUM_NODES);

      if(pReverse){
        //For all nodes
        for(int j=0; j<RouterSimulator.NUM_NODES; j++){
          // If we route via another node
          // AND we're sending to node we're routing through
          if((route[j] != j) && (route[j] == neighbours[i])){
            //tell routing node our direct path to the destination node is INF.
            distanceVector[j] = RouterSimulator.INFINITY;
          }
        }
      }
      RouterPacket packet =
          new RouterPacket(myID, neighbours[i], distanceVector);
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
    printRoute();
  }

  private void printRoute(){
    myGUI.println("\nRoute array:");

    String line = "";
    for(int i = 0; i< RouterSimulator.NUM_NODES; i++)
      line += "\t" + route[i];

    myGUI.println(line);
  }
  //--------------------------------------------------
  public void updateLinkCost(int dest, int newcost) {
    myGUI.println("Cost change! Node: " + dest);
    myGUI.println("Previous cost: " + costs[dest]);
    myGUI.println("New cost: " + newcost);
    costs[dest] = newcost;

    updateDistances();
    broadcastUpdate();
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
